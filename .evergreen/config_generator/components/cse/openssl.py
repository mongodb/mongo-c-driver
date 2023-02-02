from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks
from config_generator.etc.function import merge_defns

from config_generator.etc.cse.compile import CompileCommon
from config_generator.etc.cse.test import generate_test_tasks


SSL = 'openssl'
TAG = f'cse-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10',          'gcc',       None, ['auto']),
    ('debian11',          'gcc',       None, ['auto']),
    ('debian92',          'clang',     None, ['auto']),
    ('debian92',          'gcc',       None, ['auto']),
    ('rhel80',            'gcc',       None, ['auto']),
    ('rhel83-zseries',    'gcc',       None, ['auto']),
    ('ubuntu1604',        'clang',     None, ['auto']),
    ('ubuntu1804-arm64',  'gcc',       None, ['auto']),
    ('ubuntu1804',        'gcc',       None, ['auto']),
    ('ubuntu2004',        'gcc',       None, ['auto']),
    ('windows-64-vs2017', 'vs2017x64', None, ['auto']),
]

TEST_MATRIX = [
    ('debian10',          'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], [                            'latest']),
    ('debian11',          'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], [                            'latest']),
    ('debian92',          'clang',     None, 'auto', ['auth', 'noauth'], ['server',          ], ['4.2', '4.4', '5.0',        'latest']),
    ('debian92',          'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], ['4.2', '4.4', '5.0',        'latest']),
    ('rhel80',            'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], [                            'latest']),
    ('rhel83-zseries',    'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], [              '5.0', '6.0', 'latest']),
    ('ubuntu1604',        'clang',     None, 'auto', ['auth', 'noauth'], ['server',          ], ['4.2', '4.4',                       ]),
    ('ubuntu1804-arm64',  'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], ['4.2', '4.4', '5.0',        'latest']),
    ('ubuntu1804',        'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], ['4.2', '4.4', '5.0',                ]),
    ('ubuntu1804',        'gcc',       None, 'auto', ['auth', 'noauth'], ['server', 'replica'], [                            'latest']),
    ('ubuntu2004',        'gcc',       None, 'auto', ['auth', 'noauth'], ['server',          ], [                            'latest']),
    ('windows-64-vs2017', 'vs2017x64', None, 'auto', ['auth', 'noauth'], ['server',          ], [                            'latest']),
]
# fmt: on
# pylint: enable=line-too-long


class OpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL'


class SaslOffOpenSSLCompile(OpenSSLCompileCommon):
    name = 'cse-sasl-off-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands()


class SaslAutoOpenSSLCompile(OpenSSLCompileCommon):
    name = 'cse-sasl-auto-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands(sasl='AUTO')


def functions():
    return merge_defns(
        SaslOffOpenSSLCompile.defn(),
        SaslAutoOpenSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'off': SaslOffOpenSSLCompile,
        'auto': SaslAutoOpenSSLCompile,
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
