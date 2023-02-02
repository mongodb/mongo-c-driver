from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.compile import generate_compile_tasks
from config_generator.etc.cse.test import generate_test_tasks


SSL = 'openssl-static'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10',   'gcc',   None, ['auto']),
    ('debian11',   'gcc',   None, ['auto']),
    ('debian92',   'clang', None, ['auto']),
    ('debian92',   'gcc',   None, ['auto']),
    ('ubuntu2004', 'gcc',   None, ['auto']),
]

TEST_MATRIX = [
    ('debian10',   'gcc',   None, 'auto', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian11',   'gcc',   None, 'auto', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian92',   'clang', None, 'auto', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian92',   'gcc',   None, 'auto', ['auth', 'noauth'], ['server'], ['latest']),
    ('ubuntu2004', 'gcc',   None, 'auto', ['auth', 'noauth'], ['server'], ['latest']),
]
# fmt: on
# pylint: enable=line-too-long


class StaticOpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL_STATIC'


class SaslAutoStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'cse-sasl-auto-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands(sasl='AUTO')


def functions():
    return SaslAutoStaticOpenSSLCompile.defn()


def tasks():
    res = []

    SASL_TO_FUNC = {
        'auto': SaslAutoStaticOpenSSLCompile,
    }

    res += generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX)
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
