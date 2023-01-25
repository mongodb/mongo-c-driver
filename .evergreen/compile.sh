#!/usr/bin/env bash

if [[ "${OSTYPE}" == "cygwin" ]]; then
  bash .evergreen/compile-windows.sh
else
  bash .evergreen/compile-unix.sh
fi
