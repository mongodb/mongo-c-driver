from config_generator.etc.compile import generate_compile_tasks

from config_generator.etc.sanitizers.test import generate_test_tasks

from config_generator.components.sasl.nossl import SaslOffNoSSLCompile
from config_generator.components.sasl.openssl import SaslOffOpenSSLCompile

from config_generator.components.sanitizers.asan import TAG


# pylint: disable=line-too-long
# fmt: off
COMPILE_MATRIX = [
    ('ubuntu1604', 'clang', None, ['off']),
    ('ubuntu1804', 'clang', None, ['off']),
]

TEST_NOSSL_MATRIX = [
    ('ubuntu1604', 'clang', None, 'off', ['noauth'], ['server', 'replica', 'sharded'], ['3.6',                                            ]),
    ('ubuntu1804', 'clang', None, 'off', ['noauth'], ['server', 'replica', 'sharded'], [       '4.0', '4.2', '4.4', '5.0', '6.0', 'latest']),
]

TEST_OPENSSL_MATRIX = [
    ('ubuntu1604', 'clang', None, 'off', ['auth'], ['server', 'replica', 'sharded'], ['3.6',                                            ]),
    ('ubuntu1804', 'clang', None, 'off', ['auth'], ['server', 'replica', 'sharded'], [       '4.0', '4.2', '4.4', '5.0', '6.0', 'latest']),
]
# fmt: on
# pylint: enable=line-too-long


MORE_TAGS = ['asan']


def nossl_tasks():
    res = []

    SSL = 'nossl'
    SASL_TO_FUNC = {'off': SaslOffNoSSLCompile}

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS
    )

    res += generate_test_tasks(SSL, TAG, TEST_NOSSL_MATRIX, MORE_TAGS)

    return res


def openssl_tasks():
    res = []

    SSL = 'openssl'
    SASL_TO_FUNC = {'off': SaslOffOpenSSLCompile}

    res += generate_compile_tasks(
        SSL, TAG, SASL_TO_FUNC, COMPILE_MATRIX, MORE_TAGS
    )

    res += generate_test_tasks(SSL, TAG, TEST_OPENSSL_MATRIX, MORE_TAGS)

    return res


def tasks():
    res = []

    res += nossl_tasks()
    res += openssl_tasks()

    return res
