#!/usr/bin/env bash

set -o errexit # Exit the script with error if any of the commands fail

OS=$(uname -s | tr '[:upper:]' '[:lower:]')

case "$OS" in
cygwin*)
  bash ./.evergreen/compile-windows.sh
  ;;

*)
  bash ./.evergreen/compile-unix.sh
  ;;
esac
