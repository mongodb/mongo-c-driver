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
    ('debian10',          'gcc',        None, [       'auto']),
    ('debian11',          'gcc',        None, [       'auto']),
    ('debian81',          'clang',      None, [       'auto']),
    ('debian81',          'gcc',        None, [       'auto']),
    ('debian92',          'clang',      None, [       'auto']),
    ('debian92',          'gcc',        None, [       'auto']),
    ('rhel70',            'gcc',        None, [       'auto']),
    ('rhel80',            'gcc',        None, [       'auto']),
    ('rhel81-power8',     'gcc',        None, [       'auto']),
    ('rhel83-zseries',    'gcc',        None, [       'auto']),
    ('ubuntu1404',        'clang',      None, [       'auto']),
    ('ubuntu1404',        'gcc',        None, [       'auto']),
    ('ubuntu1604-arm64',  'gcc',        None, [       'auto']),
    ('ubuntu1604',        'clang',      None, [       'auto']),
    ('ubuntu1804-arm64',  'gcc',        None, [       'auto']),
    ('ubuntu1804',        'gcc',        None, ['off', 'auto']),
    ('ubuntu2004',        'gcc',        None, [       'auto']),
    ('windows-64-vs2017', 'vs2017x64',  None, [       'auto']),
]

TEST_MATRIX = [
    ('ubuntu1804',        'gcc',        None, 'off',  ['noauth', 'auth'], [          'replica'], [                                          'latest']),

    ('debian10',          'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [                                          'latest']),
    ('debian11',          'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [                                          'latest']),
    ('debian81',          'clang',      None, 'auto', ['noauth', 'auth'], ['server',          ], [       '4.0',                                     ]),
    ('debian81',          'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [       '4.0',                                     ]),
    ('debian92',          'clang',      None, 'auto', ['noauth', 'auth'], ['server',          ], [              '4.2', '4.4', '5.0',        'latest']),
    ('debian92',          'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [              '4.2', '4.4', '5.0',        'latest']),
    ('rhel70',            'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], ['3.6', '4.0', '4.2', '4.4', '5.0',        'latest']),
    ('rhel80',            'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [                                          'latest']),
    ('rhel81-power8',     'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [              '4.2', '4.4', '5.0',        'latest']),
    ('rhel83-zseries',    'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [                            '5.0', '6.0', 'latest']),
    ('ubuntu1404',        'clang',      None, 'auto', ['noauth', 'auth'], ['server',          ], ['3.6', '4.0',                                     ]),
    ('ubuntu1404',        'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], ['3.6', '4.0',                                     ]),
    ('ubuntu1604-arm64',  'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [       '4.0',                                     ]),
    ('ubuntu1604',        'clang',      None, 'auto', ['noauth', 'auth'], ['server',          ], ['3.6', '4.0', '4.2', '4.4',                       ]),
    ('ubuntu1804-arm64',  'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [              '4.2', '4.4', '5.0',        'latest']),
    ('ubuntu1804',        'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [              '4.2', '4.4', '5.0',                ]),
    ('ubuntu1804',        'gcc',        None, 'auto', ['noauth', 'auth'], ['server', 'replica'], [       '4.0',                             'latest']),
    ('ubuntu2004',        'gcc',        None, 'auto', ['noauth', 'auth'], ['server',          ], [                                          'latest']),
    ('windows-64-vs2017', 'vs2017x64',  None, 'auto', ['noauth', 'auth'], ['server',          ], [                                          'latest']),
]
# fmt: on
# pylint: enable=line-too-long


class OpenSSLCompileCommon(CompileCommon):
    ssl = 'OPENSSL'


class SaslOffOpenSSLCompile(OpenSSLCompileCommon):
    name = 'sasl-off-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands()


class SaslAutoOpenSSLCompile(OpenSSLCompileCommon):
    name = 'sasl-auto-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands(sasl='AUTO')


class SaslSspiOpenSSLCompile(OpenSSLCompileCommon):
    name = 'sasl-sspi-openssl-compile'
    commands = OpenSSLCompileCommon.compile_commands(sasl='SSPI')


def functions():
    return merge_defns(
        SaslOffOpenSSLCompile.defn(),
        SaslAutoOpenSSLCompile.defn(),
        SaslSspiOpenSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'off': SaslOffOpenSSLCompile,
        'auto': SaslAutoOpenSSLCompile,
        'sspi': SaslSspiOpenSSLCompile,
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
