#!/usr/bin/env bash

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
# CERT_TYPE (OCSP_ALGORITHM)
#   Required. Set to either "rsa" or "ecdsa".
# USE_DELEGATE
#   Required. May be ON or OFF. If a test requires use of a responder, this decides whether
#   the responder uses a delegate certificate. Defaults to "OFF"
#
# Example:
# TEST_COLUMN=TEST_1 CERT_TYPE=rsa USE_DELEGATE=OFF ./run-ocsp-setup.sh
#

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

check_var_req TEST_COLUMN
check_var_req CERT_TYPE
check_var_req USE_DELEGATE

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/../..")"

declare det_dir
det_dir="$(to_absolute "${mongoc_dir:?}/../drivers-evergreen-tools")"

declare server_type
case "${TEST_COLUMN:-}" in
TEST_1) server_type="valid" ;;
TEST_2) server_type="revoked" ;;
TEST_3) server_type="valid" ;;
TEST_4) server_type="revoked" ;;
MALICIOUS_SERVER_TEST_1) server_type="revoked" ;;
*) server_type="" ;;
esac

# Same responder is used for both server and client. So even stapling tests require a responder.

if [[ -n "${server_type:-}" ]]; then
  if [[ "${USE_DELEGATE}" == "ON" ]]; then
    server_type="${server_type:?}-delegate"
  fi

  OCSP_ALGORITHM="${CERT_TYPE:?}" SERVER_TYPE="${server_type:?}" bash "${det_dir}/.evergreen/ocsp/setup.sh"
fi
