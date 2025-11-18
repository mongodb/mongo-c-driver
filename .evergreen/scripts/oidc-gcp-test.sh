#!/usr/bin/env bash
set -o errexit
set -o pipefail
set -o nounset

# Install required OpenSSL runtime library.
sudo apt install -y libssl-dev

mongoc_ping=./cmake-build/src/libmongoc/mongoc-ping

export LD_LIBRARY_PATH
LD_LIBRARY_PATH="$(pwd)/cmake-build/src/libmongoc:$(pwd)/cmake-build/src/libbson:${LD_LIBRARY_PATH:-}"

echo "Testing good auth ..."
if ! "$mongoc_ping" "$MONGODB_URI?authMechanism=MONGODB-OIDC&authSource=%24external&authMechanismProperties=ENVIRONMENT:gcp,TOKEN_RESOURCE:$GCPOIDC_AUDIENCE" &>output.txt; then
  echo "mongoc-ping failed to authenticate using OIDC in GCP" 1>&2
  cat output.txt 1>&2
  exit 1
fi
echo "Testing good auth ... done"

echo "Testing bad TOKEN_RESOURCE ..."
if "$mongoc_ping" "$MONGODB_URI?authMechanism=MONGODB-OIDC&authSource=%24external&authMechanismProperties=ENVIRONMENT:gcp,TOKEN_RESOURCE:bad" &>output.txt; then
  echo "mongoc-ping unexpectedly succeeded with bad token resource" 1>&2
  exit 1
fi
echo "Testing bad TOKEN_RESOURCE ... done"
