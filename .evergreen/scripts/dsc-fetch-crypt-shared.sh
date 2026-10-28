#!/usr/bin/env bash
# Download the crypt_shared library (used by client-side encryption tests) without bootstrapping
# mongo-orchestration, and write its path to crypt-shared-expansion.yml (in the working directory).
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

python3 "${det_dir}/.evergreen/mongodl.py" \
  --component crypt_shared \
  --version "${CRYPT_SHARED_VERSION:-latest}" \
  --out "${out_dir}" \
  --empty-is-error

declare lib
lib="$(find "${out_dir}" -type f -name 'mongo_crypt_v1.so' | head -n 1)"
if [[ -z "${lib}" ]]; then
  echo "Unable to find mongo_crypt_v1.so under ${out_dir}" 1>&2
  exit 1
fi

echo "CRYPT_SHARED_LIB_PATH: \"${lib}\"" >crypt-shared-expansion.yml
cat crypt-shared-expansion.yml
