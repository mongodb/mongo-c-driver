#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
DNS=${DNS:-nodns}

echo "CC='${CC}' VALGRIND=${VALGRIND}"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json"

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SERVER_LOG="json"
export MONGOC_TEST_SKIP_MOCK="off"
export MONGOC_TEST_SKIP_LIVE="on"
export MONGOC_TEST_SKIP_SLOW="on"

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
      export PATH=$PATH:`pwd`/tests:`pwd`/Release:`pwd`/src/libbson/Release
      chmod +x ./Debug/* src/libbson/Debug/* || true
      chmod +x ./Release/* src/libbson/Release/* || true
      ;;

   darwin)
      sed -i'.bak' 's/\/data\/mci\/[a-z0-9]\{32\}\/mongoc/./g' test-libmongoc
      export DYLD_LIBRARY_PATH=".libs:src/libbson/.libs"
      ;;

   *)
      # This libtool wrapper script was built in a unique dir like
      # "/data/mci/998e754a0d1ed79b8bf733f405b87778/mongoc",
      # replace its absolute path with "." so it can run in the CWD.
      sed -i'' 's/\/data\/mci\/[a-z0-9]\{32\}\/mongoc/./g' test-libmongoc
      export LD_LIBRARY_PATH=".libs:src/libbson/.libs"
      ;;
esac

case "$OS" in
   cygwin*)
      test-libmongoc.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      if [ "$VALGRIND" = "on" ]; then
         make valgrind;
      else
         TEST_ARGS="--no-fork $TEST_ARGS"
         make -o test-libmongoc test TEST_ARGS="$TEST_ARGS"
      fi

      ;;
esac

