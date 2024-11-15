from shrub.v3.evg_build_variant import BuildVariant

from config_generator.etc.compile import generate_compile_tasks
from config_generator.etc.function import merge_defns
from config_generator.etc.utils import TaskRef

from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.test import generate_test_tasks


SSL = 'openssl'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10',          'gcc',       None, ['cyrus']),
    ('debian11',          'gcc',       None, ['cyrus']),
    ('debian92',          'clang',     None, ['cyrus']),
    ('debian92',          'gcc',       None, ['cyrus']),
    ('rhel80',            'gcc',       None, ['cyrus']),
    ('rhel8-zseries',     'gcc',       None, ['cyrus']),
    ('ubuntu2004',        'clang',     None, ['cyrus']),
    ('ubuntu2004',        'gcc',       None, ['cyrus']),
    ('ubuntu2004-arm64',  'gcc',       None, ['cyrus']),
    ('windows-vsCurrent', 'vs2017x64', None, ['cyrus']),
]

# TODO (CDRIVER-3789): test cse with the 'sharded' topology.
TEST_MATRIX = [
    # 4.2 and 4.4 not available on rhel8-zseries.
    ('rhel8-zseries', 'gcc', None, 'cyrus', ['auth'], ['server'], ['5.0']),
    
    ('windows-vsCurrent', 'vs2017x64', None, 'cyrus', ['auth'], ['server'], ['4.2', '4.4', '5.0', '6.0' ]),

    # Test 7.0+ with a replica set since Queryable Encryption does not support the 'server' topology. Queryable Encryption tests require 7.0+.
    ('ubuntu2004',        'gcc',       None, 'cyrus', ['auth'], ['server', 'replica'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
    ('rhel8-zseries',     'gcc',       None, 'cyrus', ['auth'], ['server', 'replica'], [ '7.0', '8.0', 'latest']),
    ('ubuntu2004-arm64',  'gcc',       None, 'cyrus', ['auth'], ['server', 'replica'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
    ('windows-vsCurrent', 'vs2017x64', None, 'cyrus', ['auth'], ['server', 'replica'], [ '7.0', '8.0', 'latest']),

    # Test 4.2 with Debian 10 since 4.2 does not ship on Ubuntu 20.04+.
    ('debian10',          'gcc',       None, 'cyrus', ['auth'], ['server', 'replica'], ['4.2']),
    
]
# fmt: on
# pylint: enable=line-too-long


class OpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL'


class SaslCyrusOpenSSLCompile(OpenSSLCompileCommon):
    name = 'cse-sasl-cyrus-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return merge_defns(
        SaslCyrusOpenSSLCompile.defn(),
    )


SASL_TO_FUNC = {
    'cyrus': SaslCyrusOpenSSLCompile,
}

MORE_TAGS = ['cse']

TASKS = [
    *generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS),
    *generate_test_tasks(SSL, TAG, TEST_MATRIX),
]


def tasks():
    res = TASKS.copy()

    # PowerPC and zSeries are limited resources.
    for task in res:
        if any(pattern in task.run_on for pattern in ["power", "zseries"]):
            task.patchable = False

    return res


def variants():
    expansions = {
        'CLIENT_SIDE_ENCRYPTION': 'on',
    }

    tasks = []

    # PowerPC and zSeries are limited resources.
    for task in TASKS:
        if any(pattern in task.run_on for pattern in ["power", "zseries"]):
            tasks.append(
                TaskRef(
                    name=task.name,
                    batchtime=1440,   # 1 day
                )
            )
        else:
            tasks.append(task.get_task_ref())

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=tasks,
            expansions=expansions,
        ),
    ]
