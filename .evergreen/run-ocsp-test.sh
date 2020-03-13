#! /bin/bash
# Test runner for OCSP revocation checking.
#
# Closely models the tests described in the specification:
# https://github.com/mongodb/specifications/tree/master/source/ocsp-support/tests#integration-tests-permutations-to-be-tested.
# Based on the test case, this may start a mock responder process.
# Precondition: mongod is running with the correct configuration.
#
# Environment variables:
#
# TEST_COLUMN
#   Required. Corresponds to a column of the test matrix. Set to one of the following:
#   TEST_1, TEST_2, TEST_3, TEST_4, SOFT_FAIL_TEST, MALICIOUS_SERVER_TEST_1, MALICIOUS_SERVER_TEST_2
# CERT_TYPE
#   Required. Set to either rsa or ecdsa.
# USE_DELEGATE
#   Optional. May be ON or OFF. If a test requires use of a responder, this decides whether
#   the responder uses a delegate certificate. Defaults to "OFF"
# CDRIVER_BUILD
#   Optional. The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# CDRIVER_ROOT
#   Optional. The path to mongo-c-driver source (may be same as CDRIVER_BUILD).
#   Defaults to $(pwd)
# MONGODB_PORT
#   Optional. A custom port to connect to. Defaults to 27017.
# SKIP_PIP_INSTALL
#   Optional. Skip pip install for required packages for mock responder.
#
# Example:
# TEST_COLUMN=TEST_1 CERT_TYPE=rsa ./run-ocsp-test.sh
#

# Fail on any command returning a non-zero exit status.
set -o errexit
set -o xtrace

CDRIVER_ROOT=${CDRIVER_ROOT:-$(pwd)}
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}
MONGODB_PORT=${MONGODB_PORT:-"27017"}
USE_DELEGATE=${USE_DELEGATE:-OFF}

if [ -z "$TEST_COLUMN" -o -z "$CERT_TYPE" ]; then
    echo "Required environment variable unset. See file comments for help."
    exit 1;
fi
echo "TEST_COLUMN=$TEST_COLUMN"
echo "CERT_TYPE=$CERT_TYPE"
echo "USE_DELEGATE=$USE_DELEGATE"
echo "CDRIVER_ROOT=$CDRIVER_ROOT"
echo "CDRIVER_BUILD=$CDRIVER_BUILD"
echo "MONGODB_PORT=$MONGODB_PORT"
echo "SKIP_PIP_INSTALL=$SKIP_PIP_INSTALL"

# Make paths absolute
CDRIVER_ROOT=$(cd "$CDRIVER_ROOT"; pwd)
CDRIVER_BUILD=$(cd "$CDRIVER_BUILD"; pwd)

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$OS" in
    cygwin*) OS="WINDOWS" ;;
    darwin) OS="MACOS" ;;
    *) OS="LINUX" ;;
esac

on_exit () {
    echo "Cleaning up"
    if [ -n "$RESPONDER_REQUIRED" ]; then
        echo "Responder logs:"
        cat $CDRIVER_BUILD/responder.log
        pkill -f "ocsp_mock" || true
    fi
}
trap on_exit EXIT

MONGOC_PING=$CDRIVER_BUILD/src/libmongoc/mongoc-ping
# Add libmongoc-1.0 and libbson-1.0 to library path, so mongoc-ping can find them at runtime.
if [ "$OS" = "WINDOWS" ]; then
    export PATH=$PATH:$CDRIVER_BUILD/src/libmongoc/Debug:$CDRIVER_BUILD/src/libbson/Debug
    chmod +x src/libmongoc/Debug/* src/libbson/Debug/* || true
    MONGOC_PING=$CDRIVER_BUILD/src/libmongoc/Debug/mongoc-ping.exe
elif [ "$OS" = "MACOS" ]; then
    export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$CDRIVER_BUILD/src/libmongoc:$CDRIVER_BUILD/src/libbson
elif [ "$OS" = "LINUX" ]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CDRIVER_BUILD/src/libmongoc:$CDRIVER_BUILD/src/libbson
fi

expect_success () {
    echo "Should succeed:"
    if ! $MONGOC_PING $MONGODB_URI; then
        echo "Unexpected failure"
        exit 1
    fi
}

expect_failure () {
    echo "Should fail:"
    if $MONGOC_PING $MONGODB_URI >output.txt 2>&1; then
        echo "Unexpected - succeeded but it should not have"
        exit 1
    else
        echo "failed as expected"
    fi

    # libmongoc really should give a better error message for a revocation failure...
    # It is not at all obvious what went wrong. 
    if ! grep "No suitable servers found" output.txt >/dev/null; then
        echo "Unexpected error, expecting TLS handshake failure"
        cat output.txt
        exit 1
    fi
}

# Same responder is used for both server and client. So even stapling tests require a responder.
if [ "TEST_1" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="valid"
elif [ "TEST_2" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
elif [ "TEST_3" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="valid"
elif [ "TEST_4" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
elif [ "MALICIOUS_SERVER_TEST_1" = "$TEST_COLUMN" ]; then
    RESPONDER_REQUIRED="invalid"
else
    RESPONDER_REQUIRED=""
fi

if [ -n "$RESPONDER_REQUIRED" ]; then
    echo "Starting mock responder"
    if [ -z "$SKIP_PIP_INSTALL" ]; then
        echo "Installing python dependencies"
        # Installing dependencies.
        if [ "$OS" = "WINDOWS" ]; then
            /cygdrive/c/python/Python36/python --version
            /cygdrive/c/python/Python36/python -m virtualenv venv_ocsp
            PYTHON="$(pwd)/venv_ocsp/Scripts/python"
        else
            /opt/mongodbtoolchain/v3/bin/python3 -m venv ./venv_ocsp
            PYTHON=./venv_ocsp/bin/python
        fi
        $PYTHON -m pip install oscrypto bottle asn1crypto
    fi
    cd "$CDRIVER_ROOT/.evergreen/ocsp/$CERT_TYPE"
    if [ "$RESPONDER_REQUIRED" = "invalid" ]; then
        FAULT="--fault revoked"
    fi
    if [ "ON" = "$USE_DELEGATE" ]; then
        RESPONDER_SIGNER="ocsp-responder"
    else
        RESPONDER_SIGNER="ca"
    fi
    $PYTHON ../ocsp_mock.py \
        --ca_file ca.pem \
        --ocsp_responder_cert $RESPONDER_SIGNER.crt \
        --ocsp_responder_key $RESPONDER_SIGNER.key \
        -p 8100 -v $FAULT \
        > $CDRIVER_BUILD/responder.log 2>&1 &
    cd -
fi

echo "Clearing OCSP cache for macOS/Windows"
if [ "$OS" = "MACOS" ]; then
    find ~/profile/Library/Keychains -name 'ocspcache.sqlite3' -exec sqlite3 "{}" 'DELETE FROM responses' \; || true
elif [ "$OS" = "WINDOWS" ]; then
    certutil -urlcache "*" delete || true
fi

# Always add the tlsCAFile
CA_PATH=$CDRIVER_ROOT/.evergreen/ocsp/$CERT_TYPE/ca.pem
if [ "$OS" = "WINDOWS" ]; then
    CA_PATH=$(cygpath -m -a $CA_PATH)
fi
BASE_URI="mongodb://localhost:$MONGODB_PORT/?tls=true&tlsCAFile=$CA_PATH"
MONGODB_URI="$BASE_URI"

# Only a handful of cases are expected to fail.
if [ "TEST_1" = "$TEST_COLUMN" ]; then
    expect_success
elif [ "TEST_2" = "$TEST_COLUMN" ]; then
    expect_failure
elif [ "TEST_3" = "$TEST_COLUMN" ]; then
    expect_success
elif [ "TEST_4" = "$TEST_COLUMN" ]; then
    expect_failure
elif [ "SOFT_FAIL_TEST" = "$TEST_COLUMN" ]; then
    expect_success
elif [ "MALICIOUS_SERVER_TEST_1" = "$TEST_COLUMN" ]; then
    expect_failure
elif [ "MALICIOUS_SERVER_TEST_2" = "$TEST_COLUMN" ]; then
    expect_failure
fi

# With insecure options, connection should always succeed
MONGODB_URI="$BASE_URI&tlsInsecure=true"
expect_success

# With insecure options, connection should always succeed
MONGODB_URI="$BASE_URI&tlsAllowInvalidCertificates=true"
expect_success