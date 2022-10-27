#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail


COMPRESSORS=${COMPRESSORS:-nocompressors}
AUTH=${AUTH:-noauth}
SSL=${SSL:-nossl}
URI=${URI:-}
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
DNS=${DNS:-nodns}
LOADBALANCED=${LOADBALANCED:-noloadbalanced}

# AddressSanitizer configuration
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1 symbolize=1"
export ASAN_SYMBOLIZER_PATH="/opt/mongodbtoolchain/v3/bin/llvm-symbolizer"
export TSAN_OPTIONS="suppressions=./.tsan-suppressions"

echo "COMPRESSORS='${COMPRESSORS}' CC='${CC}' AUTH=${AUTH} SSL=${SSL} URI=${URI} IPV4_ONLY=${IPV4_ONLY} VALGRIND=${VALGRIND} MONGOC_TEST_URI=${MONGOC_TEST_URI}"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json --skip-tests .evergreen/skip-tests.txt"

if [ "$COMPRESSORS" != "nocompressors" ]; then
   export MONGOC_TEST_COMPRESSORS="$COMPRESSORS"
fi
if [ "$AUTH" != "noauth" ]; then
  export MONGOC_TEST_USER="bob"
  export MONGOC_TEST_PASSWORD="pwd123"
fi

if [ "$SSL" != "nossl" ]; then
   export MONGOC_TEST_SSL_WEAK_CERT_VALIDATION="off"
   export MONGOC_TEST_SSL_PEM_FILE="src/libmongoc/tests/x509gen/client.pem"
   export MONGOC_TEST_SSL_CA_FILE="src/libmongoc/tests/x509gen/ca.pem"
   export MONGOC_TEST_CSFLE_TLS_CERTIFICATE_KEY_FILE="src/libmongoc/tests/x509gen/client.pem"
   export MONGOC_TEST_CSFLE_TLS_CA_FILE="src/libmongoc/tests/x509gen/ca.pem"
fi

export MONGOC_ENABLE_MAJORITY_READ_CONCERN=on
export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_URI="$URI"
export MONGOC_TEST_SERVER_LOG="json"
export MONGOC_TEST_SKIP_MOCK="on"
export MONGOC_TEST_IPV4_AND_IPV6_HOST="ipv4_and_ipv6.test.build.10gen.cc"

if [ "$IPV4_ONLY" != "on" ]; then
   export MONGOC_CHECK_IPV6="on"
fi

# TODO (CDRIVER-4045): consolidate DNS tests into regular test tasks.
if [ "$DNS" != "nodns" ]; then
   TEST_ARGS="$TEST_ARGS -l /initial_dns_seedlist_discovery/*"
   if [ "$DNS" = "loadbalanced" ]; then
      export MONGOC_TEST_DNS_LOADBALANCED=on
   else
      export MONGOC_TEST_DNS=on
   fi
fi

if [ "$CC" = "mingw" ]; then
   if [ "$DNS" != "nodns" ]; then
      echo "ERROR - DNS tests not implemented for MinGW yet"
      exit 1
   fi
   chmod +x ./src/libmongoc/test-libmongoc.exe
   cmd.exe /c .evergreen\\run-tests-mingw.bat
   exit 0
fi

DIR=$(dirname $0)
. $DIR/add-build-dirs-to-paths.sh
. $DIR/valgrind.sh

check_mongocryptd() {
   if [ "$CLIENT_SIDE_ENCRYPTION" = "on" -a "$ASAN" = "on" ]; then
      # ASAN does not play well with spawned processes. In addition to --no-fork, do not spawn mongocryptd
      # for client-side encryption tests.
      export "MONGOC_TEST_MONGOCRYPTD_BYPASS_SPAWN=on"
      mongocryptd --logpath ./mongocryptd.logs --fork --pidfilepath="$(pwd)/mongocryptd.pid"
      # ASAN reports an unhelpful leak of "unknown module" when linking against libmongocrypt
      # (without even calling any functions)
      # See https://github.com/google/sanitizers/issues/89 for an explanation behind this
      # workaround.
      echo "int dlclose(void *handle) { return 0; }" > bypass_dlclose.c
      "$CC" -o bypass_dlclose.so -shared bypass_dlclose.c
      export LD_PRELOAD="$(pwd)/bypass_dlclose.so:$LD_PRELOAD"
   fi
}

export MONGOC_TEST_MONITORING_VERBOSE=on

# Ensure mock KMS servers are running before starting tests.
if [ "$CLIENT_SIDE_ENCRYPTION" = "on" ]; then
   echo "Waiting for mock KMS servers to start..."
   wait_for_kms_server() {
      for i in $(seq 300); do
         # Exit code 7: "Failed to connect to host".
         if curl -s "localhost:$1"; test $? -ne 7; then
            return 0
         else
            sleep 1
         fi
      done
      echo "Could not detect mock KMS server on port $1"
      return 1
   }
   wait_for_kms_server 8999
   wait_for_kms_server 9000
   wait_for_kms_server 9001
   wait_for_kms_server 9002
   wait_for_kms_server 5698
   echo "Waiting for mock KMS servers to start... done."
   if ! test -d /cygdrive/c; then
      # We have trouble with this test on Windows. only set cryptSharedLibPath on other platforms
      export MONGOC_TEST_CRYPT_SHARED_LIB_PATH="$CRYPT_SHARED_LIB_PATH"
      echo "Setting env cryptSharedLibPath: [$MONGOC_TEST_CRYPT_SHARED_LIB_PATH]"
   fi
fi

if [ "$LOADBALANCED" != "noloadbalanced" ]; then
   if [ -z "$SINGLE_MONGOS_LB_URI" -o -z "$MULTI_MONGOS_LB_URI" ]; then
      echo "SINGLE_MONGOS_LB_URI and MULTI_MONGOS_LB_URI environment variables required."
      exit 1
   fi

   export MONGOC_TEST_LOADBALANCED=ON

   TEST_ARGS="$TEST_ARGS -l /unified/*"
   TEST_ARGS="$TEST_ARGS -l /retryable_reads/*"
   TEST_ARGS="$TEST_ARGS -l /retryable_writes/*"
   TEST_ARGS="$TEST_ARGS -l /change_streams/*"
   TEST_ARGS="$TEST_ARGS -l /loadbalanced/*"
   TEST_ARGS="$TEST_ARGS -l /load_balancers/*"
   TEST_ARGS="$TEST_ARGS -l /crud/unified/*"
   TEST_ARGS="$TEST_ARGS -l /transactions/unified/*"
   TEST_ARGS="$TEST_ARGS -l /collection-management/*"
   TEST_ARGS="$TEST_ARGS -l /sessions/unified/*"
   TEST_ARGS="$TEST_ARGS -l /change_streams/unified/*"
   TEST_ARGS="$TEST_ARGS -l /versioned_api/*"
   TEST_ARGS="$TEST_ARGS -l /command_monitoring/unified/*"
fi

case "$OS" in
   cygwin*)
      export PATH=$PATH:/cygdrive/c/mongodb/bin:/cygdrive/c/libmongocrypt/bin
      check_mongocryptd

      chmod +x src/libmongoc/Debug/test-libmongoc.exe
      ./src/libmongoc/Debug/test-libmongoc.exe $TEST_ARGS -d
      ;;

   *)
      ulimit -c unlimited || true
      # Need mongocryptd on the path.
      export PATH=$PATH:$(pwd)/mongodb/bin
      check_mongocryptd

      if [ "$VALGRIND" = "on" ]; then
         . $DIR/valgrind.sh
         run_valgrind ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS -d
      else
         ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS -d
      fi

      ;;
esac
