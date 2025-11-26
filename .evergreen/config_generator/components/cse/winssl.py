from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks
from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.test import generate_test_tasks

SSL = 'winssl'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('windows-vsCurrent', 'vs2022x64', None, ['cyrus']),
]

# QE (subset of CSFLE) requires 7.0+ and are skipped by "server" tasks.
TEST_MATRIX = [
    ('windows-vsCurrent', 'vs2022x64', None, 'cyrus', ['auth'], ['server', 'replica', 'sharded'], ['4.2', '4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),
]
# fmt: on
# pylint: enable=line-too-long


class WinSSLCompileCommon(CompileCommon):
    ssl = 'WINDOWS'


class SaslCyrusWinSSLCompile(WinSSLCompileCommon):
    name = 'cse-sasl-cyrus-winssl-compile'
    commands = WinSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return SaslCyrusWinSSLCompile.defn()


def tasks():
    res = []

    SASL_TO_FUNC = {
        'cyrus': SaslCyrusWinSSLCompile,
    }

    MORE_TAGS = ['cse']

    res += generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS)

    res += generate_test_tasks(SSL, TAG, TEST_MATRIX)

    return res


def variants():
    expansions = {
        'CLIENT_SIDE_ENCRYPTION': 'on',
    }

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
