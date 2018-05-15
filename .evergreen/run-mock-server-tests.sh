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

DIR=$(dirname $0)
. $DIR/set-path.sh

case "$OS" in
   cygwin*)
      ./src/libmongoc/test-libmongoc.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      if [ "$VALGRIND" = "on" ]; then
         . $DIR/valgrind.sh
         run_valgrind ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS
      else
         ./test-libmongoc --no-fork $TEST_ARGS
      fi

      ;;
esac

