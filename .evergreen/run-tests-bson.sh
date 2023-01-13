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

if [[ "${ASAN}" =~ "on" ]]; then
   echo "Bypassing dlclose to workaround <unknown module> ASAN warnings"
   . "$DIR/bypass-dlclose.sh"
else
   bypass_dlclose() { "$@"; } # Disable bypass otherwise.
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
      bypass_dlclose ./Debug/test-libbson.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      bypass_dlclose ./.libs/test-libbson "--no-fork $TEST_ARGS"
      ;;
esac
