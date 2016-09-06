#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail


AUTH=${AUTH:-noauth}
SSL=${SSL:-nossl}
URI=${URI:-}
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')


if [ "$AUTH" != "noauth" ]; then
  export MONGOC_TEST_USER="bob"
  export MONGOC_TEST_PASSWORD="pwd123"
fi

if [ "$SSL" != "nossl" ]; then
   export MONGOC_TEST_SSL_WEAK_CERT_VALIDATION="on"
   export MONGOC_TEST_SSL_PEM_FILE="tests/x509gen/client.pem"
   export MONGOC_TEST_SSL_CA_FILE="tests/x509gen/ca.pem"
fi

export MONGOC_ENABLE_MAJORITY_READ_CONCERN=on
export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_URI="$URI"
export MONGOC_TEST_SERVER_LOG="json"

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
      chmod +x ./Debug/* src/libbson/Debug/*
      ;;

   darwin)
      export DYLD_LIBRARY_PATH=".libs:src/libbson/.libs"
      ;;

   sunos)
      PATH="/opt/mongodbtoolchain/bin:$PATH"
      export LD_LIBRARY_PATH="/opt/csw/lib/amd64/:.libs:src/libbson/.libs"
      ;;

   *)
      # This libtool wrapper script was built in a unique dir like
      # "/data/mci/998e754a0d1ed79b8bf733f405b87778/mongoc",
      # replace its absolute path with "." so it can run in the CWD.
      sed -i'' 's/\/data\/mci\/[a-z0-9]\{32\}\/mongoc/./g' test-libmongoc
      export LD_LIBRARY_PATH=".libs:src/libbson/.libs"
      ;;
esac

#if ldconfig -N -v 2>/dev/null | grep -q libSegFault.so; then
   #export SEGFAULT_SIGNALS="all"
   #export LD_PRELOAD="libSegFault.so"
#fi

case "$OS" in
   cygwin*)
      ./Debug/test-libmongoc.exe -d -F test-results.json
      ;;

   sunos)
      gmake -o test-libmongoc test TEST_ARGS="--no-fork -d -F test-results.json"
      ;;

   *)
      make -o test-libmongoc test TEST_ARGS="--no-fork -d -F test-results.json"
      ;;
esac

