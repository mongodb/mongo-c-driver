#!/bin/sh
set -o igncr    # Ignore CR in this script
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
#       CC            Which compiler to use
#       SSL           OPENSSL, WINDOWS, or OFF
#       SASL          AUTO, SSPI, CYRUS, or OFF
#       RELEASE       Enable release-build MSVC flags (default: debug flags)


INSTALL_DIR="C:/mongoc"
CONFIGURE_FLAGS="\
   -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
   -DCMAKE_PREFIX_PATH=${INSTALL_DIR} \
   -DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF \
   -DENABLE_MAINTAINER_FLAGS=ON \
   -DENABLE_BSON=ON"
BUILD_FLAGS="/m"  # Number of concurrent processes. No value=# of cpus
CMAKE="/cygdrive/c/cmake/bin/cmake"
CC=${CC:-"Visual Studio 14 2015 Win64"}
SSL=${SSL:-WINDOWS}
SASL=${SASL:-SSPI}

echo "CC: $CC"
echo "RELEASE: $RELEASE"
echo "SASL: $SASL"

if [ "$RELEASE" ]; then
   # Build from the release tarball.
   mkdir build-dir
   tar xf ../mongoc.tar.gz -C build-dir --strip-components=1
   cd build-dir
fi

CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL=$SASL"

case "$SSL" in
   OPENSSL)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=OPENSSL"
      ;;
   WINDOWS)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=WINDOWS"
      ;;
   OFF)
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

case "$SNAPPY" in
   system)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SNAPPY=ON"
      ;;
   no)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SNAPPY=OFF"
      ;;
esac

case "$ZLIB" in
   system)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_ZLIB=SYSTEM"
      ;;
   bundled)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_ZLIB=BUNDLED"
      ;;
   no)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_ZLIB=OFF"
      ;;
esac

export CONFIGURE_FLAGS
export INSTALL_DIR

case "$CC" in
   mingw*)
      if [ "$RELEASE" ]; then
         cmd.exe /c ..\\.evergreen\\compile-windows-mingw.bat
      else
         cmd.exe /c .evergreen\\compile-windows-mingw.bat
      fi
      exit 0
   ;;
   # Resolve the compiler name to correct MSBuild location
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

if [ "$RELEASE" ]; then
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DCMAKE_BUILD_TYPE=Release"
   BUILD_FLAGS="$BUILD_FLAGS /p:Configuration=Release"
   TEST_PATH="./src/libmongoc/Release/test-libmongoc.exe"
   export PATH=$PATH:`pwd`/tests:`pwd`/Release:`pwd`/src/libbson/Release:`pwd`/src/libmongoc/Release
else
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DCMAKE_BUILD_TYPE=Debug"
   BUILD_FLAGS="$BUILD_FLAGS /p:Configuration=Debug"
   TEST_PATH="./src/libmongoc/Debug/test-libmongoc.exe"
   export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug:`pwd`/src/libmongoc/Debug
fi

"$CMAKE" -G "$CC" "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}/lib/cmake" $CONFIGURE_FLAGS
"$BUILD" $BUILD_FLAGS ALL_BUILD.vcxproj
"$BUILD" $BUILD_FLAGS INSTALL.vcxproj

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on
"$TEST_PATH" --no-fork -d -F test-results.json
