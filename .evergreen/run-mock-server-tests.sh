#!/usr/bin/env bash

set -o errexit  # Exit the script with error if any of the commands fail

OS=$(uname -s | tr '[:upper:]' '[:lower:]')
DNS=${DNS:-nodns}

echo "CC='${CC}' ASAN=${ASAN}"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json --skip-tests .evergreen/skip-tests.txt"

# AddressSanitizer configuration
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1 symbolize=1"
export ASAN_SYMBOLIZER_PATH="/usr/lib/llvm-3.4/bin/llvm-symbolizer"

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SERVER_LOG="json"
export MONGOC_TEST_SKIP_MOCK="off"
export MONGOC_TEST_SKIP_LIVE="on"
export MONGOC_TEST_SKIP_SLOW="on"

DIR=$(dirname $0)
. $DIR/add-build-dirs-to-paths.sh

if [[ "${ASAN}" =~ "on" ]]; then
   echo "Bypassing dlclose to workaround <unknown module> ASAN warnings"
   . "$DIR/bypass-dlclose.sh"
else
   bypass_dlclose() { "$@"; } # Disable bypass otherwise.
fi

case "$OS" in
   cygwin*)
      bypass_dlclose ./src/libmongoc/test-libmongoc.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      bypass_dlclose ./src/libmongoc/test-libmongoc --no-fork $TEST_ARGS
      ;;
esac
