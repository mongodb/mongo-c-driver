#!/usr/bin/env bash
set -o errexit
set -o pipefail
set -o nounset

# Working directory is expected to be mongo-c-driver repo.
ROOT=$(pwd)
INSTALL_DIR=$ROOT/install
. .evergreen/find-cmake.sh
echo "Installing libmongocrypt ... begin"
git clone --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.6.0
$CMAKE -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DBUILD_TESTING=OFF \
    "-H$ROOT/libmongocrypt" \
    "-B$ROOT/libmongocrypt"
$CMAKE --build "$ROOT/libmongocrypt" --target install
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