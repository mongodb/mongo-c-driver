#!/usr/bin/env bash
set -o errexit
set -o pipefail
set -o nounset

# Working directory is expected to be mongo-c-driver repo.
ROOT=$(pwd)
INSTALL_DIR=$ROOT/install
. .evergreen/scripts/find-cmake.sh
echo "Installing libmongocrypt ... begin"
git clone -q --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.7.0
# TODO: remove once latest libmongocrypt release contains commit 4c4aa8bf.
{
    pushd libmongocrypt
    echo "1.7.0+4c4aa8bf" >|libmongocrypt/VERSION_CURRENT
    git fetch -q origin master
    git checkout -q 4c4aa8bf # Allows -DENABLE_MONGOC=OFF.
    popd # libmongocrypt
}
MONGOCRYPT_INSTALL_PREFIX=${INSTALL_DIR} \
    DEFAULT_BUILD_ONLY=true \
    LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="-DMONGOCRYPT_MONGOC_DIR=$ROOT -DBUILD_TESTING=OFF -DENABLE_ONLINE_TESTS=OFF -DENABLE_MONGOC=OFF" \
    ./libmongocrypt/.evergreen/compile.sh
echo "Installing libmongocrypt ... end"

echo "Compile test-gcpkms ... begin"
# Disable unnecessary dependencies. test-gcpkms is copied to a remote host for testing, which may not have all dependent libraries.
$CMAKE \
    -DENABLE_SASL=OFF \
    -DENABLE_SNAPPY=OFF \
    -DENABLE_ZSTD=OFF \
    -DENABLE_ZLIB=OFF \
    -DENABLE_ICU=OFF \
    -DENABLE_SRV=OFF \
    -DENABLE_CLIENT_SIDE_ENCRYPTION=ON \
    -DCMAKE_PREFIX_PATH=$INSTALL_DIR \
    .
$CMAKE --build . --target test-gcpkms
echo "Compile test-gcpkms ... end"
