#!/bin/bash
#
# End-to-end test runner for OCSP cache.
#
# Assumptions:
#   Mongod:
#       The script assumes that a mongod is running with TLS enabled. It also assumes that the server certificate will NOT
#       staple a response. This will force the test binary to reach out to an OCSP responder to check the certificates'
#       revocation status.
#   Mock OCSP Responder:
#       This script assumes that a mock OCSP responder is running named "ocsp_mock". It also assumes that the OCSP
#       responder will respond with a certificate status of 'revoked'.
#
# Behavior:
#   This script first runs the test binary 'test-mongoc-cache' which sends a ping command to the mongod. It then waits for 5
#   seconds to give the binary enough time to make the request, and receive and process the response. Since we soft-fail
#   if an OCSP responder is not reachable, receiving a certificate status of 'revoked' is the only way we can be certain
#   our binary reached out to an OCSP responder. We assert a certificate status of 'revoked' in the test binary for both
#   ping commands.
#
#   The test binary will hang (it calls 'raise (SIGSTOP)') after the first ping. This gives us enough time to kill the
#   mock OCSP responder before sending the second ping command to the server. If the cache is used, the expected behavior,
#   then the binary will use the response cached from the first ping command and report a certificate status of 'revoked'.
#   However, if the cache is not used, then second ping command will attempt to reach out to an OCSP responder. Since the
#   only one available was killed by this script and we soft-fail if we cannot contact an OCSP responder the binary
#   will report a certificate status of "good".
#
#   The aforementioned behavior is asserted in the test binary, i.e., both ping commands should fail. If they do,
#   test-mongoc-cache will return EXIT_SUCCESS, otherwise, it will return EXIT_FAILURE.
#
# Environment variables:
#
# CDRIVER_ROOT
#   Optional. The path to mongo-c-driver source (may be same as CDRIVER_BUILD).
#   Defaults to $(pwd)
# CDRIVER_BUILD
#   Optional. The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# CERT_TYPE
#   Required. Set to either RSA or ECDSA.

set -o errexit  # Exit the script with error if any of the commands fail

CDRIVER_ROOT=${CDRIVER_ROOT:-$(pwd)}
CDRIVER_BUILD=${CDRIVER_BUILD:-$(pwd)}

if [ -z "$CERT_TYPE" ]; then
    echo "Required environment variable 'CERT_TYPE' unset. See file comments for help."
    exit 1;
fi

if [ ! `pgrep -nf mongod` ]; then
    echo "Cannot find mongod. See file comments for help."
    exit 1;
fi

if [ ! `pgrep -nf ocsp_mock` ]; then
    echo "Cannot find mock OCSP responder. See file comments for help."
    exit 1;
fi

# This test will hang after the first ping.
${CDRIVER_BUILD}/src/libmongoc/test-mongoc-cache ${CDRIVER_ROOT}/.evergreen/ocsp/${CERT_TYPE}/ca.pem &
sleep 5 # Give the program time to contact the OCSP responder

pkill -nf "ocsp_mock" # We assume that the mock OCSP responder is named "ocsp_mock"

# Resume the test binary. This will cause it to send the second ping command.
kill -s SIGCONT `pgrep -nf test-mongoc-cache`
