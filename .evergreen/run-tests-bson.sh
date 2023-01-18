#!/usr/bin/env bash

set -o errexit  # Exit the script with error if any of the commands fail


OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "CC='${CC}' ASAN='${ASAN}'"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json"

if [ "$CC" = "mingw" ]; then
   chmod +x src/libbson/test-libbson.exe
   cmd.exe /c .evergreen\\run-tests-mingw-bson.bat
   exit 0
fi

DIR=$(dirname $0)

. "$DIR/bypass-dlclose.sh"

declare ld_preload="${LD_PRELOAD:-}"
if [[ "${ASAN}" == "on" ]]; then
   ld_preload="$(bypass_dlclose):${ld_preload}"
fi

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/src/libbson/Debug
      chmod +x src/libmongoc/Debug/* src/libbson/Debug/* || true
      ;;
esac

cd src/libbson

case "$OS" in
   cygwin*)
      LD_PRELOAD="${ld_preload:-}" ./Debug/test-libbson.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      LD_PRELOAD="${ld_preload:-}" ./.libs/test-libbson "--no-fork $TEST_ARGS"
      ;;
esac
