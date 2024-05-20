#!/usr/bin/env bash
#
# Start up mongo-orchestration (a server to spawn mongodb clusters) and set up a cluster.
#
# Specify the following environment variables:
#
# MONGODB_VERSION: latest, 4.2, 4.0
# TOPOLOGY: server, replica_set, sharded_cluster
# AUTH: auth, noauth
# SSL: openssl, darwinssl, winssl, nossl
# ORCHESTRATION_FILE: <file name in DET configs/${TOPOLOGY}s/>
# REQUIRE_API_VERSION: set to a non-empty string to set the requireApiVersion parameter
#   This is currently only supported for standalone servers
# LOAD_BALANCER: off, on
#
# This script may be run locally.
#

set -x
set -o errexit  # Exit the script with error if any of the commands fail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]:?}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]:?}")/use-tools.sh" paths

: "${AUTH:="noauth"}"
: "${LOAD_BALANCER:="off"}"
: "${MONGODB_VERSION:="latest"}"
: "${ORCHESTRATION_FILE:-}"
: "${REQUIRE_API_VERSION:-}"
: "${SSL:="nossl"}"
: "${TOPOLOGY:="server"}"

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]:?}")")"

# By fetch-det.
export DRIVERS_TOOLS
DRIVERS_TOOLS="$(cd ../drivers-evergreen-tools && pwd)" # ./mongoc -> ./drivers-evergreen-tools
if [[ "${OSTYPE:?}" == cygwin ]]; then
   DRIVERS_TOOLS="$(cygpath -m "${DRIVERS_TOOLS:?}")"
fi

get_distro
get_mongodb_download_url_for "$DISTRO" "$MONGODB_VERSION"
DRIVERS_TOOLS=./ download_and_extract "$MONGODB_DOWNLOAD_URL" "$EXTRACT" "$MONGOSH_DOWNLOAD_URL" "$EXTRACT_MONGOSH"

OS=$(uname -s | tr '[:upper:]' '[:lower:]')

AUTH=${AUTH:-noauth}
SSL=${SSL:-nossl}
TOPOLOGY=${TOPOLOGY:-server}
OCSP=${OCSP:-off}
REQUIRE_API_VERSION=${REQUIRE_API_VERSION}

# If caller of script specifies an ORCHESTRATION_FILE, do not attempt to modify it. Otherwise construct it.
if [ -z "$ORCHESTRATION_FILE" ]; then
   ORCHESTRATION_FILE="basic"

   if [ "$AUTH" = "auth" ]; then
      ORCHESTRATION_FILE="auth"
   fi

   if [ "$IPV4_ONLY" = "on" ]; then
      ORCHESTRATION_FILE="${ORCHESTRATION_FILE}-ipv4-only"
   fi

   if [ -n "$AUTHSOURCE" ]; then
      ORCHESTRATION_FILE="${ORCHESTRATION_FILE}-${AUTHSOURCE}"
   fi

   if [ "$SSL" != "nossl" ]; then
      ORCHESTRATION_FILE="${ORCHESTRATION_FILE}-ssl"
   fi

   if [ "$LOAD_BALANCER" = "on" ]; then
      ORCHESTRATION_FILE="${ORCHESTRATION_FILE}-load-balancer"
   fi
fi

export MONGO_ORCHESTRATION_HOME="${DRIVERS_TOOLS:?}/.evergreen/orchestration"
export MONGODB_BINARIES="${DRIVERS_TOOLS:?}/mongodb/bin"
export PATH="${MONGODB_BINARIES:?}:$PATH"

# Workaround absence of `tls=true` URI in the `mongodb_auth_uri` field returned by mongo orchestration.
if [[ -n "${REQUIRE_API_VERSION:-}" && "${SSL:?}" != nossl ]]; then
   prev='$MONGODB_BINARIES/mongosh $URI $MONGO_ORCHESTRATION_HOME/require-api-version.js'

   # Use `--tlsAllowInvalidCertificates` to avoid self-signed certificate errors.
   next='$MONGODB_BINARIES/mongosh --tls --tlsAllowInvalidCertificates $URI $MONGO_ORCHESTRATION_HOME/require-api-version.js'

   sed -i -e "s|${prev:?}|${next:?}|" "${DRIVERS_TOOLS:?}/.evergreen/run-orchestration.sh"
fi

"${DRIVERS_TOOLS:?}/.evergreen/run-orchestration.sh"

echo "Waiting for mongo-orchestration to start..."
wait_for_mongo_orchestration() {
  declare port="${1:?"wait_for_mongo_orchestration requires a server port"}"

   for _ in $(seq 300); do
      # Exit code 7: "Failed to connect to host".
      if
         curl -s --max-time 1 "localhost:${port:?}" >/dev/null
         test $? -ne 7
      then
      if curl -s "localhost:$1" 1>|curl_mo.txt; test $? -ne 7; then
         echo "CURL SUCCESSFUL"
         return 0
      else
         echo "CURL FAILED, RETRYING"
         sleep 1
      fi
   done
   echo "Could not detect mongo-orchestration on port ${port:?}"
   return 1
}
wait_for_mongo_orchestration 8889
echo "Waiting for mongo-orchestration to start... done."

find . -name "curl_mo.txt"
echo "CATTING curl_mo.txt"
cat curl_mo.txt
ls -l curl_mo.txt

python -m json.tool curl_mo.txt
sleep 5
pwd
curl -s --data @"$ORCHESTRATION_FILE" "$ORCHESTRATION_URL" 1>|curl_mo.txt
python -m json.tool curl_mo.txt
sleep 15

if [ "$AUTH" = "auth" ]; then
  MONGO_SHELL_CONNECTION_FLAGS="--username bob --password pwd123"
fi

if [ -n "$AUTHSOURCE" ]; then
   MONGO_SHELL_CONNECTION_FLAGS="${MONGO_SHELL_CONNECTION_FLAGS} --authenticationDatabase ${AUTHSOURCE}"
fi

if [ "$OCSP" != "off" ]; then
   MONGO_SHELL_CONNECTION_FLAGS="${MONGO_SHELL_CONNECTION_FLAGS} --host localhost --tls --tlsAllowInvalidCertificates"
elif [ "$SSL" != "nossl" ]; then
   MONGO_SHELL_CONNECTION_FLAGS="${MONGO_SHELL_CONNECTION_FLAGS} --host localhost --ssl --sslCAFile=$MONGO_ORCHESTRATION_HOME/lib/ca.pem --sslPEMKeyFile=$MONGO_ORCHESTRATION_HOME/lib/client.pem"
fi

if [ ! -z "$REQUIRE_API_VERSION" ]; then
  MONGO_SHELL_CONNECTION_FLAGS="${MONGO_SHELL_CONNECTION_FLAGS} --apiVersion=1"
  # Set the requireApiVersion parameter.
  ./mongodb/bin/mongosh $MONGO_SHELL_CONNECTION_FLAGS $DIR/../etc/require-api-version.js
fi

echo $MONGO_SHELL_CONNECTION_FLAGS

# Create mo-expansion.yml. expansions.update expects the file to exist.
touch mo-expansion.yml

if [ -z "$MONGO_CRYPT_SHARED_DOWNLOAD_URL" ]; then
  echo "There is no crypt_shared library for distro='$DISTRO' and version='$MONGODB_VERSION'".
else
  echo "Downloading crypt_shared package from $MONGO_CRYPT_SHARED_DOWNLOAD_URL"
  download_and_extract_crypt_shared "$MONGO_CRYPT_SHARED_DOWNLOAD_URL" "$EXTRACT" "CRYPT_SHARED_LIB_PATH"
  echo "CRYPT_SHARED_LIB_PATH: $CRYPT_SHARED_LIB_PATH"
  if [ -z "$CRYPT_SHARED_LIB_PATH" ]; then
    echo "CRYPT_SHARED_LIB_PATH must be assigned, but wasn't" 1>&2 # write to stderr"
    exit 1
  fi
cat >>mo-expansion.yml <<EOT
CRYPT_SHARED_LIB_PATH: "$CRYPT_SHARED_LIB_PATH"
EOT

fi
