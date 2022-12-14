#! /bin/bash
# Run an OCSP mock responder server if necessary.
#
# See the tests described in the specification for more info:
# https://github.com/mongodb/specifications/tree/master/source/ocsp-support/tests#integration-tests-permutations-to-be-tested.
# Precondition: mongod is NOT running. The responder should be started first.
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
#
# Example:
# TEST_COLUMN=TEST_1 CERT_TYPE=rsa ./run-ocsp-test.sh
#

# Fail on any command returning a non-zero exit status.
set -o errexit

CDRIVER_ROOT=${CDRIVER_ROOT:-$(pwd)}
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}
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

# Make paths absolute
CDRIVER_ROOT=$(cd "$CDRIVER_ROOT"; pwd)
CDRIVER_BUILD=$(cd "$CDRIVER_BUILD"; pwd)

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
case "$OS" in
    cygwin*) OS="WINDOWS" ;;
    darwin) OS="MACOS" ;;
    *) OS="LINUX" ;;
esac

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

# Same responder is used for both server and client. So even stapling tests require a responder.

if [ -n "$RESPONDER_REQUIRED" ]; then
    echo "Starting mock responder"
    if [ ! -d "../drivers-evergreen-tools" ]; then
        git clone --depth=1 git@github.com:mongodb-labs/drivers-evergreen-tools.git ../drivers-evergreen-tools
    fi

    pushd ../drivers-evergreen-tools/.evergreen/ocsp
    . ./activate-ocspvenv.sh
    popd # ../drivers-evergreen-tools/.evergreen/ocsp

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
