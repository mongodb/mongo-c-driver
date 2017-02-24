#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

CMAKE=/opt/cmake/bin/cmake
if command -v gtar 2>/dev/null; then
   TAR=gtar
else
   TAR=tar
fi

SRCROOT=`pwd`

BUILD_DIR=$(pwd)/cbuild
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

INSTALL_DIR=$(pwd)/install-dir
rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

cd $BUILD_DIR
$TAR xf ../../mongoc.tar.gz -C . --strip-components=1
CONFIGURE_FLAGS="-DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF"

cd src/libbson
$CMAKE "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" $CONFIGURE_FLAGS
make
make install

cd $BUILD_DIR
$CMAKE "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" $CONFIGURE_FLAGS ..
make
make install

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on
export CTEST_OUTPUT_ON_FAILURE=1

LD_LIBRARY_PATH=$INSTALL_DIR/lib make test

# Test our CMake package, libmongoc-1.0-config.cmake.
cd $SRCROOT/examples/cmake/find_package
$CMAKE -DCMAKE_PREFIX_PATH=$INSTALL_DIR/lib/cmake .
make
LD_LIBRARY_PATH=$INSTALL_DIR/lib ./hello_mongoc

# Test our pkg-config file, libmongoc-1.0.pc.
cd $SRCROOT/examples/cmake/FindPkgConfig
PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig $CMAKE .
make
LD_LIBRARY_PATH=$INSTALL_DIR/lib ./hello_mongoc
