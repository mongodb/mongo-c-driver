#!/bin/sh
set -o igncr    # Ignore CR in this script
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
#  CC              Which compiler to use
#  SSL             OPENSSL, OPENSSL_STATIC, WINDOWS, or OFF
#  SASL            AUTO, SSPI, CYRUS, or OFF
#  SRV             Whether to enable SRV: ON or OFF
#  RELEASE         Enable release-build MSVC flags (default: debug flags)
#  SKIP_MOCK_TESTS Skips running the libmongoc mock server tests after compiling


INSTALL_DIR="$(cygpath -a ./install-dir -w)"
CONFIGURE_FLAGS="\
   -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
   -DCMAKE_PREFIX_PATH=${INSTALL_DIR} \
   -DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF \
   -DENABLE_MAINTAINER_FLAGS=ON \
   -DENABLE_BSON=ON"
BUILD_FLAGS="/m"  # Number of concurrent processes. No value=# of cpus
CMAKE="/cygdrive/c/cmake/bin/cmake"
CC=${CC:-"Visual Studio 15 2017 Win64"}
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
   OPENSSL_STATIC)
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=OPENSSL -DOPENSSL_USE_STATIC_LIBS=ON"
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

if [ "$SRV" = "OFF" ]; then
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SRV=OFF"
fi

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
esac

if [ "$RELEASE" ]; then
   BUILD_CONFIG="RelWithDebInfo"
   TEST_PATH="./src/libmongoc/RelWithDebInfo/test-libmongoc.exe"
   export PATH=$PATH:`pwd`/src/libbson/RelWithDebInfo:`pwd`/src/libmongoc/RelWithDebInfo:`pwd`/install-dir/bin
else
   CONFIGURE_FLAGS="${CONFIGURE_FLAGS} -DENABLE_DEBUG_ASSERTIONS=ON"
   BUILD_CONFIG="Debug"
   TEST_PATH="./src/libmongoc/Debug/test-libmongoc.exe"
   export PATH=$PATH:`pwd`/src/libbson/Debug:`pwd`/src/libmongoc/Debug:`pwd`/install-dir/bin
fi

if [ "$COMPILE_LIBMONGOCRYPT" = "ON" ]; then
   # Build libmongocrypt, using the previously fetched installed source.
   git clone https://github.com/mongodb/libmongocrypt
   mkdir libmongocrypt/cmake-build
   cd libmongocrypt/cmake-build
   "$CMAKE" -G "$CC" "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}/lib/cmake" -DENABLE_SHARED_BSON=ON -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" ../
   "$CMAKE" --build . --target INSTALL --config $BUILD_CONFIG -- $BUILD_FLAGS
   cd ../../
fi

"$CMAKE" -G "$CC" "-DCMAKE_PREFIX_PATH=${INSTALL_DIR}/lib/cmake" $CONFIGURE_FLAGS
"$CMAKE" --build . --target ALL_BUILD --config $BUILD_CONFIG -- $BUILD_FLAGS
"$CMAKE" --build . --target INSTALL --config $BUILD_CONFIG -- $BUILD_FLAGS

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on

# We are done here if we don't want to run the tests.
if [ "$SKIP_MOCK_TESTS" = "ON" ]; then
   exit 0
fi

export MONGOC_TEST_SERVER_LOG=stdout

"$TEST_PATH" --no-fork -d -F test-results.json
