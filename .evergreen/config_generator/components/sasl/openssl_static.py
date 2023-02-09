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
  ('debian92',      'gcc',   None, ['auto']),
  ('debian92',      'clang', None, ['auto']),
  ('debian10',      'gcc',   None, ['auto']),
  ('debian11',      'gcc',   None, ['auto']),
  ('ubuntu2004',    'gcc',   None, ['auto']),
]

TEST_MATRIX = [
  ('debian92',      'clang', None, 'auto', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian92',      'gcc',   None, 'auto', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian10',      'gcc',   None, 'auto', ['noauth', 'auth'], ['server'], ['latest']),
  ('debian11',      'gcc',   None, 'auto', ['noauth', 'auth'], ['server'], ['latest']),
  ('ubuntu2004',    'gcc',   None, 'auto', ['noauth', 'auth'], ['server'], ['latest']),
]
# fmt: on
# pylint: enable=line-too-long


class StaticOpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL_STATIC'


class SaslOffStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'sasl-off-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands()


class SaslAutoStaticOpenSSLCompile(StaticOpenSSLCompileCommon):
    name = 'sasl-auto-openssl-static-compile'
    commands = StaticOpenSSLCompileCommon.compile_commands(sasl='AUTO')


def functions():
    return merge_defns(
        SaslOffStaticOpenSSLCompile.defn(),
        SaslAutoStaticOpenSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'off': SaslOffStaticOpenSSLCompile,
        'auto': SaslAutoStaticOpenSSLCompile,
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
