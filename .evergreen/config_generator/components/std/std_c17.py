from shrub.v3.evg_build_variant import BuildVariant
from shrub.v3.evg_task import EvgTaskRef

from config_generator.etc.compile import generate_compile_tasks

from config_generator.components.sasl.openssl import SaslCyrusOpenSSLCompile


TAG = 'std-matrix-c17'


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('debian10', 'gcc', None, ['cyrus']),
    ('debian11', 'gcc', None, ['cyrus']),
]
# fmt: on
# pylint: enable=line-too-long


def tasks():
    res = []

    SSL = 'openssl'
    SASL_TO_FUNC = {'cyrus': SaslCyrusOpenSSLCompile}

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS=['std-c17']
    )

    return res


def variants():
    expansions = {
        # CMake 3.21 or newer is required for CMAKE_C_STANDARD=17 or newer.
        # Use explicit `-std=` flags instead until newer CMake is available.
        'CFLAGS': '-std=c17',
    }

    return [
        BuildVariant(
            name=TAG,
            display_name=TAG,
            tasks=[EvgTaskRef(name=f'.{TAG}')],
            expansions=expansions,
        ),
    ]
