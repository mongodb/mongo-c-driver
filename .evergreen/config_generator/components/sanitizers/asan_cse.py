from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sanitizers.test import generate_test_tasks

from config_generator.components.cse.openssl import SaslCyrusOpenSSLCompile

from config_generator.components.sanitizers.asan import TAG


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('ubuntu1804', 'clang', None, ['cyrus']),
]

TEST_MATRIX = [
    ('ubuntu1804', 'clang', None, 'cyrus', ['auth'], ['server',          ], ['4.2', '4.4', '5.0',                ]),
    ('ubuntu1804', 'clang', None, 'cyrus', ['auth'], ['server', 'replica'], [                     '6.0', 'latest']),
]
# fmt: on
# pylint: enable=line-too-long


MORE_TAGS = ['cse', 'asan']


def tasks():
    res = []

    SSL = 'openssl'
    SASL_TO_FUNC = {
        'cyrus': SaslCyrusOpenSSLCompile,
    }

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS
    )

    res += generate_test_tasks(SSL, TAG, TEST_MATRIX, MORE_TAGS)

    res += generate_test_tasks(
        SSL, TAG, TEST_MATRIX, MORE_TAGS,
        MORE_TEST_TAGS=['with-mongocrypt'],
        MORE_VARS={'SKIP_CRYPT_SHARED_LIB': 'on'}
    )

    return res
