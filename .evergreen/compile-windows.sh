#!/bin/sh
set -o igncr    # Ignore CR in this script
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
#       CC            Which compiler to use
#       SSL           Which SSL Library to use
#       SASL          Enable or disable SASL


CONFIGURE_FLAGS="-DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF"
case "$SASL" in
   no)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL:BOOL=OFF"
   ;;
   sasl)
   case "$CC" in
      *Win64)
         CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL=CYRUS"
      ;;
      *)
         CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL:BOOL=OFF"
      ;;
   esac
   ;;
   sspi)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL=SSPI"
   ;;
   *)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL:BOOL=OFF"
   ;;
esac

case "$SSL" in
   openssl)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=OPENSSL"
      ;;
   winssl)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=WINDOWS"
      ;;
   no)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL:BOOL=OFF"
      ;;
   *)
   case "$CC" in
      *Win64)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS"
      ;;
      *)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL:BOOL=OFF"
      ;;
   esac
esac

# Resolve the compiler name to correct MSBuild location
case "$CC" in
   "Visual Studio 10 2010")
      BUILD="/cygdrive/c/Windows/Microsoft.NET/Framework/v4.0.30319/MSBuild.exe"
   ;;
   "Visual Studio 10 2010 Win64")
      BUILD="/cygdrive/c/Windows/Microsoft.NET/Framework64/v4.0.30319/MSBuild.exe"
   ;;
   "Visual Studio 12 2013")
      BUILD="/cygdrive/c/Program Files (x86)/MSBuild/12.0/Bin/MSBuild.exe"
   ;;
   "Visual Studio 12 2013 Win64")
      BUILD="/cygdrive/c/Program Files (x86)/MSBuild/12.0/Bin/MSBuild.exe"
   ;;
   "Visual Studio 14 2015")
      BUILD="/cygdrive/c/Program Files (x86)/MSBuild/14.0/Bin/MSBuild.exe"
   ;;
   "Visual Studio 14 2015 Win64")
      BUILD="/cygdrive/c/Program Files (x86)/MSBuild/14.0/Bin/MSBuild.exe"
   ;;
esac

export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
CMAKE="/cygdrive/c/cmake/bin/cmake"
INSTALL_DIR="C:/mongoc"

# CMake can't compile against bundled libbson, so we have to
# compile it and install it seperately, and the configure mongoc
# to build against the installed libbson
git submodule update --init
cd src/libbson
"$CMAKE" -G "$CC" "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" $CONFIGURE_FLAGS
"$BUILD" /m ALL_BUILD.vcxproj
"$BUILD" /m INSTALL.vcxproj
cd ../..

"$CMAKE" -G "$CC" "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" "-DBSON_ROOT_DIR=${INSTALL_DIR}" $CONFIGURE_FLAGS
"$BUILD" /m ALL_BUILD.vcxproj
"$BUILD" /m INSTALL.vcxproj

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on
./Debug/test-libmongoc.exe --no-fork -d -F test-results.json
