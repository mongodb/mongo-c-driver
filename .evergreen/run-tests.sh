#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail


COMPRESSORS=${COMPRESSORS:-nocompressors}
AUTH=${AUTH:-noauth}
SSL=${SSL:-nossl}
URI=${URI:-}
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
DNS=${DNS:-nodns}

# AddressSanitizer configuration
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1 symbolize=1"
export ASAN_SYMBOLIZER_PATH="/opt/mongodbtoolchain/v3/bin/llvm-symbolizer"
export TSAN_OPTIONS="suppressions=./.tsan-suppressions"

echo "COMPRESSORS='${COMPRESSORS}' CC='${CC}' AUTH=${AUTH} SSL=${SSL} URI=${URI} IPV4_ONLY=${IPV4_ONLY} VALGRIND=${VALGRIND} MONGOC_TEST_URI=${MONGOC_TEST_URI}"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json"

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
   sudo cp src/libmongoc/tests/x509gen/ca.pem /usr/local/share/ca-certificates/cdriver.crt || true
   if [ -f /usr/local/share/ca-certificates/cdriver.crt ]; then
      sudo update-ca-certificates
   else
      export MONGOC_TEST_SSL_CA_FILE="src/libmongoc/tests/x509gen/ca.pem"
   fi
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

if [ "$DNS" = "dns-auth" ]; then
   export MONGOC_TEST_DNS=on
   TEST_ARGS="$TEST_ARGS -l /initial_dns_auth/*"
elif [ "$DNS" != "nodns" ]; then
   export MONGOC_TEST_DNS=on
   TEST_ARGS="$TEST_ARGS -l /initial_dns_seedlist_discovery*"
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

