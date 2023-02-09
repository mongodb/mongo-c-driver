from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.function import merge_defns
from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sasl.compile import CompileCommon
from config_generator.etc.sasl.test import generate_test_tasks


SSL = 'openssl-static'
TAG = f'sasl-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
  ('debian92',      'gcc',   None, ['cyrus']),
  ('debian92',      'clang', None, ['cyrus']),
  ('debian10',      'gcc',   None, ['cyrus']),
  ('debian11',      'gcc',   None, ['cyrus']),
  ('ubuntu2004',    'gcc',   None, ['cyrus']),
]

TEST_MATRIX = [
  ('debian92',      'clang', None, 'cyrus', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian92',      'gcc',   None, 'cyrus', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian10',      'gcc',   None, 'cyrus', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian11',      'gcc',   None, 'cyrus', ['noauth', 'auth'], ['server'], ['latest']),
  ('ubuntu2004',    'gcc',   None, 'cyrus', ['noauth', 'auth'], ['server'], ['latest']),
]
# fmt: on
# pylint: enable=line-too-long


class StaticOpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL_STATIC'


class SaslOffStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'sasl-off-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands()


class SaslCyrusStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'sasl-cyrus-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return merge_defns(
        SaslOffStaticOpenSSLCompile.defn(),
        SaslCyrusStaticOpenSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'off': SaslOffStaticOpenSSLCompile,
        'cyrus': SaslCyrusStaticOpenSSLCompile,
    }

    res += generate_compile_tasks(SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX)
    res += generate_test_tasks(SSL, TAG, TEST_MATRIX)

    return res


def variants():
    expansions = {
        'DEBUG': 'ON'
    }

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
