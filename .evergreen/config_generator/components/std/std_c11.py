from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks

from config_generator.components.sasl.openssl import SaslCyrusOpenSSLCompile


TAG = 'std-matrix-c11'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('archlinux',  'clang', None,   ['cyrus']),
    ('debian81',   'clang', None,   ['cyrus']),
    ('debian92',   'clang', None,   ['cyrus']),
    ('ubuntu1604', 'clang', 'i686', ['cyrus']),
    ('ubuntu1604', 'clang', None,   ['cyrus']),
    ('ubuntu1804', 'clang', 'i686', ['cyrus']),
    ('ubuntu1804', 'gcc',   None,   ['cyrus']),
    ('debian10',   'clang', None,   ['cyrus']),
    ('debian10',   'gcc',   None,   ['cyrus']),
    ('debian11',   'clang', None,   ['cyrus']),
    ('debian11',   'gcc',   None,   ['cyrus']),
    ('ubuntu2004', 'clang', None,   ['cyrus']),
    ('ubuntu2004', 'gcc',   None,   ['cyrus']),
]
# fmt: on
# pylint: enable=line-too-long


def tasks():
    res = []

    SSL = 'openssl'
    SASL_TO_FUNC = {'cyrus': SaslCyrusOpenSSLCompile}

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS=['std-c11']
    )

    return res


def variants():
    expansions = {
        'C_STD_VERSION': '11',
    }

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
