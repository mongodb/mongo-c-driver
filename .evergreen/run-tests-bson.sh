#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail


OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "CC='${CC}'"

[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')
TEST_ARGS="-d -F test-results.json"

if [ "$CC" = "mingw" ]; then
   chmod +x src/libbson/test-libbson.exe
   cmd.exe /c .evergreen\\run-tests-mingw-bson.bat
   exit 0
fi

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
      chmod +x ./Debug/* src/libbson/Debug/* || true
      ;;
esac

cd src/libbson

case "$OS" in
   cygwin*)
      ./Debug/test-libbson.exe $TEST_ARGS
      ;;

   *)
      ulimit -c unlimited || true

      if [ "$VALGRIND" = "on" ]; then
         make valgrind
      else
         ./.libs/test-libbson "--no-fork $TEST_ARGS"
      fi
      ;;
esac

