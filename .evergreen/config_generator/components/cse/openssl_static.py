from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.test import generate_test_tasks


SSL = 'openssl-static'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10',   'gcc',   None, ['cyrus']),
    ('debian11',   'gcc',   None, ['cyrus']),
    ('debian92',   'clang', None, ['cyrus']),
    ('debian92',   'gcc',   None, ['cyrus']),
    ('ubuntu2004', 'gcc',   None, ['cyrus']),
]

TEST_MATRIX = [
    ('debian10',   'gcc',   None, 'cyrus', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian11',   'gcc',   None, 'cyrus', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian92',   'clang', None, 'cyrus', ['auth', 'noauth'], ['server'], ['latest']),
    ('debian92',   'gcc',   None, 'cyrus', ['auth', 'noauth'], ['server'], ['latest']),
    ('ubuntu2004', 'gcc',   None, 'cyrus', ['auth', 'noauth'], ['server'], ['latest']),
]
# fmt: on
# pylint: enable=line-too-long


class StaticOpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL_STATIC'


class SaslCyrusStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'cse-sasl-cyrus-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return SaslCyrusStaticOpenSSLCompile.defn()


def tasks():
    res = []

    SASL_TO_FUNC = {
        'cyrus': SaslCyrusStaticOpenSSLCompile,
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
