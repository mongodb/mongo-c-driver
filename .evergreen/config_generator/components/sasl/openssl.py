from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.function import merge_defns
from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sasl.compile import CompileCommon
from config_generator.etc.sasl.test import generate_test_tasks


SSL = 'openssl'
TAG = f'sasl-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10',          'gcc',        None, ['cyrus']),
    ('debian11',          'gcc',        None, ['cyrus']),
    ('debian92',          'clang',      None, ['cyrus']),
    ('debian92',          'gcc',        None, ['cyrus']),
    ('rhel80',            'gcc',        None, ['cyrus']),
    ('rhel81-power8',     'gcc',        None, ['cyrus']),
    ('rhel83-zseries',    'gcc',        None, ['cyrus']),
    ('ubuntu2004',        'clang',        None, ['cyrus']),
    ('ubuntu2004-arm64',  'gcc',        None, ['cyrus']),
    ('ubuntu2004',        'gcc',        None, ['cyrus']),
    ('windows-vsCurrent', 'vs2017x64',  None, ['cyrus']),
]

TEST_MATRIX = [
    ('rhel81-power8',     'gcc',       None, 'cyrus', ['auth'], ['server',          ], [       '4.2', '4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
    ('rhel83-zseries',    'gcc',       None, 'cyrus', ['auth'], ['server',          ], [                     '5.0', '6.0', '7.0', '8.0', 'latest']),

    ('ubuntu2004-arm64',  'gcc',       None, 'cyrus', ['auth'], ['server'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
    ('ubuntu2004',        'gcc',       None, 'cyrus', ['auth'], ['server'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
    ('windows-vsCurrent', 'vs2017x64', None, 'cyrus', ['auth'], ['server'], [       'latest']),

    # Test 4.2 with Debian 10 since 4.2 does not ship on Ubuntu 20.04+.
    ('debian10',          'gcc',       None, 'cyrus', ['auth'], ['server', 'replica'], ['4.2']), 
]
# fmt: on
# pylint: enable=line-too-long


class OpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL'


class SaslCyrusOpenSSLCompile(OpenSSLCompileCommon):
    name = 'sasl-cyrus-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return merge_defns(
        SaslCyrusOpenSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'cyrus': SaslCyrusOpenSSLCompile,
    }

    res += generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX)
    res += generate_test_tasks(SSL, TAG, TEST_MATRIX)

    return res


def variants():
    expansions = {}

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
