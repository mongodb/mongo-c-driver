from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.test import generate_test_tasks


SSL = 'darwinssl'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('macos-1014', 'clang', None, ['auto']),
]

TEST_MATRIX = [
    ('macos-1014', 'clang', None, 'auto', ['auth', 'noauth'], ['server'], ['4.2', '4.4', '5.0', 'latest']),
]
# fmt: on
# pylint: enable=line-too-long


class DarwinSSLCompileCommon(CompileCommon):
    ssl = 'DARWIN'


class SaslAutoDarwinSSLCompile(DarwinSSLCompileCommon):
    name = 'cse-sasl-auto-darwinssl-compile'
    commands = DarwinSSLCompileCommon.compile_commands(sasl='AUTO')


def functions():
    return SaslAutoDarwinSSLCompile.defn()


def tasks():
    res = []

    SASL_TO_FUNC = {
        'auto': SaslAutoDarwinSSLCompile,
    }

    MORE_TAGS = ['cse']

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS
    )

    res += generate_test_tasks(SSL, TAG, TEST_MATRIX)

    return res


def variants():
    expansions = {
        'CLIENT_SIDE_ENCRYPTION': 'on',
        'DEBUG': 'ON',
    }

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
