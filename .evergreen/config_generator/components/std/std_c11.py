from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks

from config_generator.components.sasl.openssl import SaslAutoOpenSSLCompile


TAG = 'std-matrix-c11'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('archlinux',  'clang', None,   ['auto']),
    ('debian81',   'clang', None,   ['auto']),
    ('debian92',   'clang', None,   ['auto']),
    ('ubuntu1604', 'clang', 'i686', ['auto']),
    ('ubuntu1604', 'clang', None,   ['auto']),
    ('ubuntu1804', 'clang', 'i686', ['auto']),
    ('ubuntu1804', 'gcc',   None,   ['auto']),
]
# fmt: on
# pylint: enable=line-too-long


def tasks():
    res = []

    SSL = 'openssl'
    SASL_TO_FUNC = {'auto': SaslAutoOpenSSLCompile}

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
