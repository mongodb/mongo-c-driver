#!/usr/bin/env bash
set -o errexit
set -o pipefail
set -o nounset

# Working directory is expected to be mongo-c-driver repo.
ROOT=$(pwd)
INSTALL_DIR=$ROOT/install
. .evergreen/scripts/find-cmake-latest.sh
declare cmake_binary
cmake_binary="$(find_cmake_latest)"
echo "Installing libmongocrypt ... begin"
git clone --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.7.0
# TODO: remove once latest libmongocrypt release contains commit c6f65fe6.
{
    pushd libmongocrypt
    echo "1.7.0+c6f65fe6" >|VERSION_CURRENT
    git fetch -q origin master
    git checkout -q c6f65fe6 # Allows -DENABLE_MONGOC=OFF.
    popd # libmongocrypt
}
DEBUG="0" \
    CMAKE_EXE="${cmake_binary}" \
    MONGOCRYPT_INSTALL_PREFIX=${INSTALL_DIR} \
    DEFAULT_BUILD_ONLY=true \
    LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="-DMONGOCRYPT_MONGOC_DIR=$ROOT -DBUILD_TESTING=OFF -DENABLE_ONLINE_TESTS=OFF -DENABLE_MONGOC=OFF" \
    ./libmongocrypt/.evergreen/compile.sh
echo "Installing libmongocrypt ... end"

echo "Compile test-azurekms ... begin"
# Disable unnecessary dependencies. test-azurekms is copied to a remote host for testing, which may not have all dependent libraries.
"${cmake_binary}" \
    -DENABLE_SASL=OFF \
    -DENABLE_SNAPPY=OFF \
    -DENABLE_ZSTD=OFF \
    -DENABLE_ZLIB=OFF \
    -DENABLE_ICU=OFF \
    -DENABLE_SRV=OFF \
    -DENABLE_CLIENT_SIDE_ENCRYPTION=ON \
    -DCMAKE_PREFIX_PATH=$INSTALL_DIR \
    .
"${cmake_binary}" --build . --target test-azurekms
echo "Compile test-azurekms ... end"
