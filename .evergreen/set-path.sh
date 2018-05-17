#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail

set_path ()
{
   case "$OS" in
      cygwin*)
         export PATH=$PATH:`pwd`/src/libbson/Debug
         export PATH=$PATH:`pwd`/src/libbson/Release
         chmod +x src/libmongoc/Debug/* src/libbson/Debug/* || true
         chmod +x src/libmongoc/Release/* src/libbson/Release/* || true
         ;;

      darwin)
         export DYLD_LIBRARY_PATH=".:install-dir/lib:src/libbson:src/libmongoc:$EXTRA_LIB_PATH:$DYLD_LIBRARY_PATH"
         ;;

      *)
         export LD_LIBRARY_PATH=".:install-dir/lib:src/libbson:src/libmongoc:$EXTRA_LIB_PATH:$LD_LIBRARY_PATH"
         ;;
   esac
}

set_path
