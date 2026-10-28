#!/usr/bin/env bash
# Download the crypt_shared library and the mongocryptd binary (used by client-side encryption tests) without
# bootstrapping mongo-orchestration, and write the crypt_shared path to crypt-shared-expansion.yml (in the working
# directory). Most tests do not load crypt_shared and instead spawn mongocryptd from PATH; run-tests.sh adds
# drivers-evergreen-tools/mongodb/bin to PATH.
#
# Inputs (environment):
#   CRYPT_SHARED_VERSION  Server version to download crypt_shared for (default: latest).
#   AWS_*                 Credentials for the private "latest" bucket (see DRIVERS-3628).
#
# Expects to be run from the mongoc directory, with drivers-evergreen-tools as a sibling.

set -o errexit
set -o pipefail

declare det_dir
det_dir="$(cd ../drivers-evergreen-tools && pwd)"

declare out_dir="${det_dir}/mongodb"
mkdir -p "${out_dir}"

# mongodl.py needs certifi and boto3 (the "latest" download path reads a private S3 bucket); the host's system Python
# does not have them, so run it through uv.
uv run --no-project --with certifi --with boto3 python "${det_dir}/.evergreen/mongodl.py" \
  --component crypt_shared \
  --version "${CRYPT_SHARED_VERSION:-latest}" \
  --out "${out_dir}" \
  --empty-is-error

# Extract only mongocryptd from the server archive, as mongodb/bin/mongocryptd.
uv run --no-project --with certifi --with boto3 python "${det_dir}/.evergreen/mongodl.py" \
  --component archive \
  --version "${CRYPT_SHARED_VERSION:-latest}" \
  --out "${out_dir}" \
  --only "*/bin/mongocryptd" \
  --strip-path-components 1 \
  --empty-is-error

if [[ ! -x "${out_dir}/bin/mongocryptd" ]]; then
  echo "Unable to find an executable mongocryptd at ${out_dir}/bin/mongocryptd" 1>&2
  exit 1
fi

declare lib
lib="$(find "${out_dir}" -type f -name 'mongo_crypt_v1.so' | head -n 1)"
if [[ -z "${lib}" ]]; then
  echo "Unable to find mongo_crypt_v1.so under ${out_dir}" 1>&2
  exit 1
fi

echo "CRYPT_SHARED_LIB_PATH: \"${lib}\"" >crypt-shared-expansion.yml
cat crypt-shared-expansion.yml
