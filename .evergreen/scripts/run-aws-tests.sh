#!/usr/bin/env bash

# Test runner for AWS authentication.
#
# This script is meant to be run in parts (so to isolate the AWS tests).
# Pass the desired test first argument (REGULAR, EC2, ECS, ASSUME_ROLE, LAMBDA)
#
# Example:
# run-aws-tests.sh EC2
#
# Optional environment variables:
#
# drivers_tools_dir
#   The path to clone of https://github.com/mongodb-labs/drivers-evergreen-tools.
#   Defaults to $(pwd)../drivers-evergreen-tools
# mongoc_dir
#   The path to the build of mongo-c-driver (e.g. mongo-c-driver/cmake-build).
#   Defaults to $(pwd)
# mongoc_dir
#   The path to mongo-c-driver source (may be same as mongoc_dir).
#   Defaults to $(pwd)
# mongodb_bin_dir
#   The path to mongodb binaries.
#   Defaults to $(pwd)/mongodb/bin
# iam_auth_ecs_account and iam_auth_ecs_secret_access_key
#   Set to access key id/secret access key. Required for some tests.

set -o errexit
set -o pipefail

# Do not trace
set +o xtrace

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"

check_var_req TESTCASE

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/../..")"

declare drivers_tools_dir
drivers_tools_dir="$(to_absolute "${mongoc_dir}/../drivers-evergreen-tools")"

declare mongodb_bin_dir="${mongoc_dir}/mongodb/bin"
declare mongoc_ping="${mongoc_dir}/src/libmongoc/mongoc-ping"

# Add libmongoc-1.0 and libbson-1.0 to library path, so mongoc-ping can find them at runtime.
if [[ "${OSTYPE}" == "cygwin" ]]; then
  export PATH
  PATH+=":${mongoc_dir}/src/libmongoc/Debug"
  PATH+=":${mongoc_dir}/src/libbson/Debug"

  chmod -f +x src/libmongoc/Debug/* src/libbson/Debug/* || true

  mongoc_ping="${mongoc_dir}/src/libmongoc/Debug/mongoc-ping.exe"
elif [[ "${OSTYPE}" == darwin* ]]; then
  export DYLD_LIBRARY_PATH
  DYLD_LIBRARY_PATH+=":${mongoc_dir}/src/libmongoc"
  DYLD_LIBRARY_PATH+=":${mongoc_dir}/src/libbson"
else
  export LD_LIBRARY_PATH
  LD_LIBRARY_PATH+=":${mongoc_dir}/src/libmongoc"
  LD_LIBRARY_PATH+=":$mongoc_dir/src/libbson"
fi

expect_success() {
  echo "Should succeed:"
  if ! "${mongoc_ping}" "${1:?}"; then
    echo "Unexpected auth failure" 1>&2
    exit 1
  fi
}

expect_failure() {
  echo "Should fail:"
  if "${mongoc_ping}" "${1:?}" >output.txt 2>&1; then
    echo "Unexpected - authed but it should not have" 1>&2
    exit 1
  else
    echo "auth failed as expected"
  fi

  if ! grep "Authentication failed" output.txt >/dev/null; then
    echo "Unexpected, error was not an authentication failure:" 1>&2
    cat output.txt 1>&2
    exit 1
  fi
}

url_encode() {
  declare encoded=""
  for c in $(echo ${1:?} | grep -o .); do
    case "${c}" in
    [a-zA-Z0-9.~_-])
      encoded="${encoded}${c}"
      ;;
    *)
      encoded="${encoded}$(printf '%%%02X' "'${c}")"
      ;;
    esac
  done
  echo "${encoded}"
}

# Some of the setup scripts expect mongo to be on path.
export PATH
PATH+=":${mongodb_bin_dir}"

if [[ "${TESTCASE}" == "REGULAR" ]]; then
  echo "===== Testing regular auth via URI ====="

  # Create user on $external db.
  pushd "${drivers_tools_dir}/.evergreen/auth_aws"
  mongo --verbose aws_e2e_regular_aws.js
  popd # "${drivers_tools_dir}/.evergreen/auth_aws"

  declare user_encoded pass_encoded
  user_encoded="$(url_encode "${iam_auth_ecs_account:?}")"
  pass_encoded="$(url_encode "${iam_auth_ecs_secret_access_key:?}")"

  expect_success "mongodb://${user_encoded:?}:${pass_encoded:?}@localhost/?authMechanism=MONGODB-AWS"
  expect_failure "mongodb://${user_encoded:?}:bad_password@localhost/?authMechanism=MONGODB-AWS"

  exit
fi

if [[ "${TESTCASE}" == "ASSUME_ROLE" ]]; then
  echo "===== Testing auth with session token via URI with AssumeRole ====="
  pushd "${drivers_tools_dir}/.evergreen/auth_aws"
  mongo --verbose aws_e2e_assume_role.js
  popd # "${drivers_tools_dir}/.evergreen/auth_aws"

  declare user pass token
  user="$(jq -r '.AccessKeyId' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"
  pass="$(jq -r '.SecretAccessKey' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"
  token="$(jq -r '.SessionToken' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"

  declare user_encoded pass_encoded token_encoded
  user_encoded="$(url_encode "${user:?}")"
  pass_encoded="$(url_encode "${pass:?}")"
  token_encoded="$(url_encode "${token:?}")"

  expect_success "mongodb://${user_encoded}:${pass_encoded}@localhost/aws?authMechanism=MONGODB-AWS&authSource=\$external&authMechanismProperties=AWS_SESSION_TOKEN:${token_encoded}"
  expect_failure "mongodb://${user_encoded}:${pass_encoded}@localhost/aws?authMechanism=MONGODB-AWS&authSource=\$external&authMechanismProperties=AWS_SESSION_TOKEN:bad_token"
  exit
fi

if [[ "LAMBDA" = "$TESTCASE" ]]; then
  echo "===== Testing auth via environment variables ====="

  pushd "${drivers_tools_dir}/.evergreen/auth_aws"
  mongo --verbose aws_e2e_assume_role.js
  popd # "${drivers_tools_dir}/.evergreen/auth_aws"

  declare user pass token
  user="$(jq -r '.AccessKeyId' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"
  pass="$(jq -r '.SecretAccessKey' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"
  token="$(jq -r '.SessionToken' "${drivers_tools_dir}/.evergreen/auth_aws/creds.json")"

  echo "Valid credentials - should succeed:"
  export AWS_ACCESS_KEY_ID="${user:?}"
  export AWS_SECRET_ACCESS_KEY="${pass:?}"
  export AWS_SESSION_TOKEN="${token:?}"
  expect_success "mongodb://localhost/?authMechanism=MONGODB-AWS"
  exit
fi

if [[ "${TESTCASE}" == "EC2" ]]; then
  echo "===== Testing auth via EC2 task metadata ====="
  # Do necessary setup for EC2
  # Create user on $external db.
  pushd "${drivers_tools_dir}/.evergreen/auth_aws"
  mongo --verbose aws_e2e_ec2.js
  popd # "${drivers_tools_dir}/.evergreen/auth_aws"

  echo "Valid credentials via EC2 - should succeed"
  expect_success "mongodb://localhost/?authMechanism=MONGODB-AWS"
  exit
fi

if [[ "${TESTCASE}" == "ECS" ]]; then
  echo "===== Testing auth via ECS task metadata ====="
  [[ -d "${drivers_tools_dir}" ]]
  # Overwrite the test that gets run by remote ECS task.
  cp "${mongoc_dir}/.evergreen/ecs_hosted_test.js" "${drivers_tools_dir}/.evergreen/auth_aws/lib"
  chmod 777 "${script_dir}/run-mongodb-aws-ecs-test.sh"

  pushd "${drivers_tools_dir}/.evergreen/auth_aws"

  cat <<EOF >setup.js
    const mongo_binaries = "${mongodb_bin_dir}";
    const project_dir = "${mongoc_dir}";
EOF

  "${mongodb_bin_dir}/mongo" --nodb setup.js aws_e2e_ecs.js
  exit
fi

echo "Unexpected testcase '${TESTCASE}'" 1>&2
exit 1
