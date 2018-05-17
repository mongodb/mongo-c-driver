#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail


COMPRESSORS=${COMPRESSORS:-nocompressors}
AUTH=${AUTH:-noauth}
SSL=${SSL:-nossl}
URI=${URI:-}
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
DNS=${DNS:-nodns}

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
   export MONGOC_TEST_SSL_WEAK_CERT_VALIDATION="on"
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
. $DIR/set-path.sh
. $DIR/valgrind.sh

case "$OS" in
   cygwin*)
      chmod +x src/libmongoc/Debug/test-libmongoc.exe
      ./src/libmongoc/Debug/test-libmongoc.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      if [ "$VALGRIND" = "on" ]; then
         . $DIR/valgrind.sh
         run_valgrind ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS
      else
         ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS
      fi

      ;;
esac

