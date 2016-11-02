#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

CMAKE=/opt/cmake/bin/cmake

cpus=$(grep -c '^processor' /proc/cpuinfo)
MAKEFLAGS="-j${cpus}"
export CTEST_OUTPUT_ON_FAILURE=1


INSTALL_DIR=$(pwd)/install-dir
rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

BUILD_DIR=$(pwd)/cbuild
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

CONFIGURE_FLAGS="-DENABLE_AUTOMATIC_INIT_AND_CLEANUP:BOOL=OFF"

cd src/libbson
$CMAKE "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" "-DBSON_ROOT_DIR=${INSTALL_DIR}" $CONFIGURE_FLAGS
make
make install

cd $BUILD_DIR
$CMAKE "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}" "-DBSON_ROOT_DIR=${INSTALL_DIR}" $CONFIGURE_FLAGS ..
make

export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on

make test
make install
