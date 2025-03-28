#!/usr/bin/env bash
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
#   LINK_STATIC              Whether to statically link to libbson
#   BUILD_SAMPLE_WITH_CMAKE  Link program w/ CMake. Default: use pkg-config.


echo "LINK_STATIC=$LINK_STATIC BUILD_SAMPLE_WITH_CMAKE=$BUILD_SAMPLE_WITH_CMAKE"

DIR=$(dirname $0)
. $DIR/find-cmake-latest.sh
CMAKE=$(find_cmake_latest)
. $DIR/check-symlink.sh

# The major version of the project. Appears in certain install filenames.
_full_version=$(cat "$DIR/../../VERSION_CURRENT")
version="${_full_version%-*}"  # 1.2.3-dev → 1.2.3
major="${version%%.*}"         # 1.2.3     → 1
echo "major version: $version"
echo " full version: $major"

# Get the kernel name, lowercased
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
echo "OS: $OS"

if [ "$OS" = "darwin" ]; then
  SO=dylib
  LIB_SO=libbson$major.$version.dylib
  LDD="otool -L"
else
  SO=so
  LIB_SO=libbson$major.so.$version
  LDD=ldd
fi

SRCROOT=`pwd`
SCRATCH_DIR=$(pwd)/.scratch
rm -rf "$SCRATCH_DIR"
mkdir -p "$SCRATCH_DIR"
cp -r -- "$SRCROOT"/* "$SCRATCH_DIR"

BUILD_DIR=$SCRATCH_DIR/build-dir
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

INSTALL_DIR=$SCRATCH_DIR/install-dir
rm -rf $INSTALL_DIR
mkdir -p $INSTALL_DIR

cd $BUILD_DIR

# Use ccache if able.
if [[ -f $DIR/find-ccache.sh ]]; then
  . $DIR/find-ccache.sh
  find_ccache_and_export_vars "$SCRATCH_DIR" || true
fi

if [ "$LINK_STATIC" ]; then
  # Our CMake system builds shared and static by default.
  $CMAKE -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DBUILD_TESTING=OFF -DENABLE_TESTS=OFF -DENABLE_MONGOC=OFF "$SCRATCH_DIR"
  $CMAKE --build . --parallel
  $CMAKE --build . --parallel --target install
else
  $CMAKE -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DBUILD_TESTING=OFF -DENABLE_TESTS=OFF -DENABLE_MONGOC=OFF -DENABLE_STATIC=OFF "$SCRATCH_DIR"
  $CMAKE --build . --parallel
  $CMAKE --build . --parallel --target install

  set +o xtrace

  if test -f $INSTALL_DIR/lib/libbson-static-1.0.a; then
    echo "libbson-static-1.0.a shouldn't have been installed"
    exit 1
  fi
  if test -f $INSTALL_DIR/lib/libbson-1.0.a; then
    echo "libbson-1.0.a shouldn't have been installed"
    exit 1
  fi
  if test -f $INSTALL_DIR/lib/pkgconfig/libbson-static-1.0.pc; then
    echo "libbson-static-1.0.pc shouldn't have been installed"
    exit 1
  fi

fi

# Revert ccache options, they no longer apply.
unset CCACHE_BASEDIR CCACHE_NOHASHDIR

ls -l $INSTALL_DIR/lib

set +o xtrace

# Check on Linux that libbson is installed into lib/ like:
# libbson-1.0.so -> libbson-1.0.so.0
# libbson-1.0.so.0 -> libbson-1.0.so.0.0.0
# libbson-1.0.so.0.0.0
if [ "$OS" != "darwin" ]; then
  # From check-symlink.sh
  check_symlink libbson$major.so        libbson$major.so.$major
  check_symlink libbson$major.so.$major libbson$major.so.$version
  SONAME=$(objdump -p $INSTALL_DIR/lib/$LIB_SO|grep SONAME|awk '{print $2}')
  EXPECTED_SONAME="libbson$major.so.$major"
  if [ "$SONAME" != "$EXPECTED_SONAME" ]; then
    echo "SONAME should be $EXPECTED_SONAME, not $SONAME"
    exit 1
  else
    echo "library name check ok, SONAME=$SONAME"
  fi
else
  # Just test that the shared lib was installed.
  if test ! -f $INSTALL_DIR/lib/$LIB_SO; then
    echo "$LIB_SO missing!"
    exit 1
  else
    echo "$LIB_SO check ok"
  fi
fi

if test ! -f $INSTALL_DIR/lib/pkgconfig/bson$major.pc; then
  echo "bson$major.pc missing!"
  exit 1
else
  echo "bson$major.pc check ok"
fi
if test ! -f $INSTALL_DIR/lib/cmake/bson-$version/bsonConfig.cmake; then
  echo "bsonConfig.cmake missing!"
  exit 1
else
  echo "bsonConfig.cmake check ok"
fi
if test ! -f $INSTALL_DIR/lib/cmake/bson-$version/bsonConfig.cmake; then
  echo "bsonConfig.cmake missing!"
  exit 1
else
  echo "bsonConfig.cmake check ok"
fi

if [ "$LINK_STATIC" ]; then
  if test ! -f $INSTALL_DIR/lib/libbson$major.a; then
    echo "libbson$major.a missing!"
    exit 1
  else
    echo "libbson$major.a check ok"
  fi
  if test ! -f $INSTALL_DIR/lib/pkgconfig/bson$major-static.pc; then
    echo "bson$major-static.pc missing!"
    exit 1
  else
    echo "bson$major-static.pc check ok"
  fi
fi

cd $SRCROOT

if [ "$BUILD_SAMPLE_WITH_CMAKE" ]; then
  EXAMPLE_DIR=$SRCROOT/src/libbson/examples/cmake/find_package

  if [ "$LINK_STATIC" ]; then
    EXAMPLE_DIR="${EXAMPLE_DIR}_static"
  fi

  cd $EXAMPLE_DIR
  $CMAKE -DCMAKE_PREFIX_PATH=$INSTALL_DIR/lib/cmake .
  $CMAKE --build . --parallel
else
  # Test our pkg-config file.
  export PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig
  cd $SRCROOT/src/libbson/examples

  if [ "$LINK_STATIC" ]; then
    echo "pkg-config output:"
    echo $(pkg-config --libs --cflags bson$major-static)
    env major=$major ./compile-with-pkg-config-static.sh
  else
    echo "pkg-config output:"
    echo $(pkg-config --libs --cflags bson$major)
    env major=$major ./compile-with-pkg-config.sh
  fi
fi

if [ ! "$LINK_STATIC" ]; then
  if [ "$OS" = "darwin" ]; then
    export DYLD_LIBRARY_PATH=$INSTALL_DIR/lib
  else
    export LD_LIBRARY_PATH=$INSTALL_DIR/lib
  fi
fi

echo "ldd hello_bson:"
$LDD hello_bson

./hello_bson
