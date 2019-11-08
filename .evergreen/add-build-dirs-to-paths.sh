#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail

set_path ()
{
   case "$OS" in
      cygwin*)
         export PATH=$PATH:`pwd`/src/libbson/Debug
         export PATH=$PATH:`pwd`/src/libbson/Release
         export PATH=$PATH:`pwd`/install-dir/bin
         chmod +x src/libmongoc/Debug/* src/libbson/Debug/* || true
         chmod +x src/libmongoc/Release/* src/libbson/Release/* || true
         chmod +x `pwd`/install-dir/bin/* || true
         ;;

      darwin)
         export DYLD_LIBRARY_PATH=".:install-dir/lib:install-dir/lib64:src/libbson:src/libmongoc:$EXTRA_LIB_PATH:$DYLD_LIBRARY_PATH"
         ;;

      *)
         export LD_LIBRARY_PATH=".:install-dir/lib:install-dir/lib64:src/libbson:src/libmongoc:$EXTRA_LIB_PATH:$LD_LIBRARY_PATH"
         ;;
   esac

   case "$OS" in
      cygwin*)
         ;;

      *)
         export PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig:$PKG_CONFIG_PATH
         export PATH=$INSTALL_DIR/bin:$PATH
         ;;
   esac
}

set_path
