from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sanitizers.test import generate_test_tasks

from config_generator.components.cse.openssl import SaslCyrusOpenSSLCompile

from config_generator.components.sanitizers.asan import TAG


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('ubuntu2004', 'clang', None, ['cyrus']),
    ('debian10',   'clang', None, ['cyrus']),
]

TEST_MATRIX = [
    # Test 7.0+ with a replica set since Queryable Encryption does not support the 'server' topology. Queryable Encryption tests require 7.0+.
    ('ubuntu2004', 'clang', None, 'cyrus', ['auth'], ['server', 'replica'], ['4.4', '5.0', '6.0', '7.0', '8.0', 'latest']),

    # Test 4.2 with Debian 10 since 4.2 does not ship on Ubuntu 20.04+.
    ('debian10',   'clang', None, 'cyrus', ['auth'], ['server'], ['4.2']), 
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
