from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.function import merge_defns
from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sasl.compile import CompileCommon
from config_generator.etc.sasl.test import generate_test_tasks


SSL = 'nossl'
TAG = f'sasl-matrix-{SSL}'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('archlinux',         'clang',       None,   ['off']),
    ('debian10',          'gcc',         None,   ['off']),
    ('debian11',          'gcc',         None,   ['off']),
    ('debian92',          'clang',       None,   ['off']),
    ('debian92',          'gcc',         None,   ['off']),
    ('macos-1014',        'clang',       None,   ['off']),
    ('rhel70',            'gcc',         None,   ['off']),
    ('rhel80',            'gcc',         None,   ['off']),
    ('rhel81-power8',     'gcc',         None,   ['off']),
    ('rhel83-zseries',    'gcc',         None,   ['off']),
    ('ubuntu1604',        'clang',       'i686', ['off']),
    ('ubuntu1604',        'gcc',         None,   ['off']),
    ('ubuntu1804-arm64',  'gcc',         None,   ['off']),
    ('ubuntu1804',        'clang',       'i686', ['off']),
    ('ubuntu1804',        'gcc',         'i686', ['off']),
    ('ubuntu1804',        'gcc',         None,   ['off']),
    ('ubuntu2004',        'gcc',         None,   ['off']),
    ('windows-64-vs2017', 'mingw',       None,   ['off']),
    ('windows-64-vs2017', 'vs2017x64',   None,   ['off']),
    ('windows-64-vs2017', 'vs2017x86',   None,   ['off']),
]

TEST_MATRIX = [
    ('ubuntu1604', 'gcc', None, 'off', ['noauth'], ['server', 'replica', 'sharded'], ['3.6',                                     ]),
    ('ubuntu1804', 'gcc', None, 'off', ['noauth'], ['server', 'replica', 'sharded'], [       '4.0', '4.2', '4.4', '5.0', 'latest']),
]
# fmt: on
# pylint: enable=line-too-long


class NoSSLCompileCommon(CompileCommon):
    ssl = 'OFF'


class SaslOffNoSSLCompile(NoSSLCompileCommon):
    name = 'sasl-off-nossl-compile'
    commands = NoSSLCompileCommon.compile_commands(sasl='OFF')


class SaslCyrusNoSSLCompile(NoSSLCompileCommon):
    name = 'sasl-cyrus-nossl-compile'
    commands = NoSSLCompileCommon.compile_commands(sasl='CYRUS')


def functions():
    return merge_defns(
        SaslOffNoSSLCompile.defn(),
        SaslCyrusNoSSLCompile.defn(),
    )


def tasks():
    res = []

    SASL_TO_FUNC = {
        'off': SaslOffNoSSLCompile,
        'cyrus': SaslCyrusNoSSLCompile,
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
