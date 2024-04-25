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
# Setup OIDC token in /tmp/tokens/
export AWS_PROFILE="drivers-test-secrets-role-857654397073"
$DIR/../../../drivers-evergreen-tools/.evergreen/auth_oidc/oidc_get_tokens.sh
./setup-oidc-tokens.sh

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
         return 0
      else
         sleep 1
      fi
   done
   echo "Could not detect mongo-orchestration on port ${port:?}"
   return 1
}
wait_for_mongo_orchestration 8889
echo "Waiting for mongo-orchestration to start... done."
