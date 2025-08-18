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
    # For test matrix.
    ('rhel8-latest',      'gcc',       None, ['cyrus']),
    ('rhel8-zseries',     'gcc',       None, ['cyrus']), # Big Endian.
    ('ubuntu2004-arm64',  'gcc',       None, ['cyrus']),
    ('windows-vsCurrent', 'vs2022x64', None, ['cyrus']),

    # For compile only.
    ('debian11',   'clang',    None, ['cyrus']),
    ('debian11',   'gcc',      None, ['cyrus']),
    ('debian12',   'clang',    None, ['cyrus']),
    ('debian12',   'gcc',      None, ['cyrus']),
    ('rhel80',     'gcc',      None, ['cyrus']),
    ('ubuntu2004', 'gcc',      None, ['cyrus']),
    ('ubuntu2004', 'clang',    None, ['cyrus']),
    ('ubuntu2204', 'gcc',      None, ['cyrus']),
    ('ubuntu2204', 'clang-12', None, ['cyrus']),
    ('ubuntu2404', 'gcc',      None, ['cyrus']),
    ('ubuntu2404', 'clang-14', None, ['cyrus']),
]

# QE (subset of CSFLE) requires 7.0+ and are skipped by "server" tasks.
TEST_MATRIX = [
    ('rhel8-latest', 'gcc', None, 'cyrus', ['auth'], ['server', 'replica', 'sharded'], ['4.2', '4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),

    ('windows-vsCurrent', 'vs2022x64', None, 'cyrus', ['auth'], ['server', 'replica', 'sharded'], ['4.2', '4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),

    # ubuntu2004-arm64 only provides 4.4+.
    ('ubuntu2004-arm64', 'gcc', None, 'cyrus', ['auth'], ['server', 'replica', 'sharded'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),

    # rhel8-zseries only provides 5.0+. Resource-limited: use sparingly.
    ('rhel8-zseries', 'gcc', None, 'cyrus', ['auth'], ['sharded'], ['5.0', 'latest']),
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

    tasks.sort(key=lambda t: t.name)

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=tasks,
            expansions=expansions,
        ),
    ]
