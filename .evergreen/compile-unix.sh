#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
# Options for this script:
#       CFLAGS                  Additional compiler flags
#       MARCH                   Machine Architecture. Defaults to lowercase uname -m
#       RELEASE                 Use the fully qualified release archive
#       DEBUG                   Use debug configure flags
#       TRACING                 Use function tracing
#       VALGRIND                Run the test suite through valgrind
#       CC                      Which compiler to use
#       ANALYZE                 Run the build through clangs scan-build
#       COVERAGE                Produce code coverage reports
#       RDTSCP                  Use Intel RDTSCP instruction
#       SKIP_MOCK_TESTS         Skips running the libmongoc mock server tests after compiling
# Options for CMake:
#       LIBBSON                 Build against bundled or external libbson
#       EXTRA_CONFIGURE_FLAGS   Extra configure flags to use
#       EXTRA_CMAKE_PREFIX_PATH Extra directories to search for libraries/packages
#       ZLIB                    Build against bundled or external zlib, or none
#       SNAPPY                  Build against Snappy, or none
#       SSL                     Build against OpenSSL or native or none
#       SASL                    Build against SASL or not
#       SRV                     Whether to enable SRV: ON or OFF
#       ENABLE_SHM_COUNTERS     Build with SHM counters
#       ZSTD                    Build against system zstd.

# Options for this script.
RELEASE=${RELEASE:-OFF}
DEBUG=${DEBUG:-OFF}
TRACING=${TRACING:-OFF}
VALGRIND=${VALGRIND:-OFF}
ANALYZE=${ANALYZE:-OFF}
COVERAGE=${COVERAGE:-OFF}
RDTSCP=${RDTSCP:-OFF}
SKIP_MOCK_TESTS=${SKIP_MOCK_TESTS:-OFF}
ENABLE_SHM_COUNTERS=${ENABLE_SHM_COUNTERS:-AUTO}

# CMake options.
SASL=${SASL:-OFF}
SSL=${SSL:-OFF}
SNAPPY=${SNAPPY:-AUTO}
ZLIB=${ZLIB:-BUNDLED}
INSTALL_DIR=$(pwd)/install-dir

echo "CFLAGS: $CFLAGS"
echo "MARCH: $MARCH"
echo "RELEASE: $RELEASE"
echo "DEBUG: $DEBUG"
echo "TRACING: $TRACING"
echo "VALGRIND: $VALGRIND"
echo "CC: $CC"
echo "ANALYZE: $ANALYZE"
echo "COVERAGE: $COVERAGE"
echo "SKIP_MOCK_TESTS: $SKIP_MOCK_TESTS"
echo "ZLIB: $ZLIB"
echo "ZSTD: $ZSTD"

# Get the kernel name, lowercased
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
echo "OS: $OS"

# Since zstd inconsitently installed on macos-1014.
# Remove this check in CDRIVER-3483.
if [ "darwin" = "$OS" ]; then
   ZSTD="OFF"
fi

# Automatically retrieve the machine architecture, lowercase, unless provided
# as an environment variable (e.g. to force 32bit)
[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')

CMAKE_PREFIX_PATH="$INSTALL_DIR"
if [ ! -z "$EXTRA_CMAKE_PREFIX_PATH" ]; then
   CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH;$EXTRA_CMAKE_PREFIX_PATH"
fi

# Default CMake flags for debug builds and release builds.
# CMAKE_SKIP_RPATH avoids hardcoding absolute paths to dependency libraries.
DEBUG_AND_RELEASE_FLAGS="\
   -DCMAKE_SKIP_RPATH=TRUE \
   -DENABLE_BSON=ON \
   -DENABLE_MAN_PAGES=OFF \
   -DENABLE_HTML_DOCS=OFF \
   -DENABLE_MAINTAINER_FLAGS=ON \
   -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF \
   -DENABLE_TRACING=$TRACING \
   -DENABLE_RDTSCP=$RDTSCP \
   -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH \
   -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
   -DENABLE_SHM_COUNTERS=$ENABLE_SHM_COUNTERS \
"

if [ ! -z "$ZLIB" ]; then
   DEBUG_AND_RELEASE_FLAGS="$DEBUG_AND_RELEASE_FLAGS -DENABLE_ZLIB=${ZLIB}"
fi

if [ ! -z "$SNAPPY" ]; then
   DEBUG_AND_RELEASE_FLAGS="$DEBUG_AND_RELEASE_FLAGS -DENABLE_SNAPPY=${SNAPPY}"
fi

if [ ! -z "$ZSTD" ]; then
   DEBUG_AND_RELEASE_FLAGS="$DEBUG_AND_RELEASE_FLAGS -DENABLE_ZSTD=${ZSTD}"
fi

DEBUG_FLAGS="${DEBUG_AND_RELEASE_FLAGS} -DCMAKE_BUILD_TYPE=Debug"
RELEASE_FLAGS="${DEBUG_AND_RELEASE_FLAGS} -DCMAKE_BUILD_TYPE=RelWithDebInfo"

# Where are we, without relying on realpath or readlink?
if [ "$(dirname $0)" = "." ]; then
   DIR="$(pwd)"
elif [ "$(dirname $0)" = ".." ]; then
   DIR="$(dirname "$(pwd)")"
else
   DIR="$(cd "$(dirname "$0")"; pwd)"
fi

. $DIR/find-cmake.sh

# --strip-components is an GNU tar extension. Check if the platform
# has GNU tar installed as `gtar`, otherwise we assume to be on
# platform that supports it
# command -v returns success error code if found and prints the path to it
if command -v gtar 2>/dev/null; then
   TAR=gtar
else
   TAR=tar
fi

[ "$DEBUG" = "ON" ] && CONFIGURE_FLAGS=$DEBUG_FLAGS || CONFIGURE_FLAGS=$RELEASE_FLAGS

CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SASL=${SASL}"
if [ "${SSL}" = "OPENSSL_STATIC" ]; then
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=OPENSSL -DOPENSSL_USE_STATIC_LIBS=ON"
else
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=${SSL}"
fi
[ "$COVERAGE" = "ON" ] && CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_COVERAGE=ON -DENABLE_EXAMPLES=OFF"

if [ "$RELEASE" = "ON" ]; then
   # Build from the release tarball.
   mkdir build-dir
   $TAR xf ../mongoc.tar.gz -C build-dir --strip-components=1
   cd build-dir
else
  CONFIGURE_FLAGS="${CONFIGURE_FLAGS} -DENABLE_DEBUG_ASSERTIONS=ON"
fi

if [ "$SRV" = "OFF" ]; then
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SRV=OFF"
fi

# UndefinedBehaviorSanitizer configuration
export UBSAN_OPTIONS="print_stacktrace=1 abort_on_error=1"
# AddressSanitizer configuration
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1 symbolize=1"
export ASAN_SYMBOLIZER_PATH="/opt/mongodbtoolchain/v3/bin/llvm-symbolizer"
export TSAN_OPTIONS="suppressions=./.tsan-suppressions"

case "$MARCH" in
   i386)
      CFLAGS="$CFLAGS -m32 -march=i386"
      CXXFLAGS="$CXXFLAGS -m32 -march=i386"
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SNAPPY=AUTO -DENABLE_ZLIB=BUNDLED -DENABLE_ZSTD=OFF"
   ;;
   s390x)
      CFLAGS="$CFLAGS -march=z196 -mtune=zEC12"
      CXXFLAGS="$CXXFLAGS -march=z196 -mtune=zEC12"
   ;;
   x86_64)
      CFLAGS="$CFLAGS -m64 -march=x86-64"
      CXXFLAGS="$CXXFLAGS -m64 -march=x86-64"
   ;;
   ppc64le)
      CFLAGS="$CFLAGS -mcpu=power8 -mtune=power8 -mcmodel=medium"
      CXXFLAGS="$CXXFLAGS -mcpu=power8 -mtune=power8 -mcmodel=medium"
   ;;
esac

case "$OS" in
   darwin)
      CFLAGS="$CFLAGS -Wno-unknown-pragmas"
      # llvm-cov is installed from brew
      export PATH=$PATH:/usr/local/opt/llvm/bin
   ;;
esac

case "$CC" in
   clang)
      CXX=clang++
   ;;
   gcc)
      CXX=g++
   ;;
esac

CONFIGURE_FLAGS="$CONFIGURE_FLAGS $EXTRA_CONFIGURE_FLAGS"
export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on
export MONGOC_TEST_IPV4_AND_IPV6_HOST="ipv4_and_ipv6.test.build.10gen.cc"

export CFLAGS="$CFLAGS"
export CXXFLAGS="$CXXFLAGS"
export CC="$CC"
export CXX="$CXX"

export PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

export PATH=$INSTALL_DIR/bin:$PATH
echo "OpenSSL Version:"
pkg-config --modversion libssl || true

if [ "$COMPILE_LIBMONGOCRYPT" = "ON" ]; then
   # Build libmongocrypt, using the previously fetched installed source.
   git clone https://github.com/mongodb/libmongocrypt
   mkdir libmongocrypt/cmake-build
   cd libmongocrypt/cmake-build
   $CMAKE -DENABLE_SHARED_BSON=ON -DCMAKE_BUILD_TYPE="Debug" -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" ../
   make install
   cd ../../
   else
   # Hosts may have libmongocrypt installed from apt/yum. We do not want to pick those up
   # since those libmongocrypt packages statically link libbson.
   CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_CLIENT_SIDE_ENCRYPTION=OFF"
fi

if [ "$ANALYZE" = "ON" ]; then
   # Clang static analyzer, available on Ubuntu 16.04 images.
   # https://clang-analyzer.llvm.org/scan-build.html
   #
   # On images other than Ubuntu 16.04, use scan-build-3.9 if
   # scan-build is not found.
   if command -v scan-build 2>/dev/null; then
      SCAN_BUILD_COMMAND="scan-build"
   else
      SCAN_BUILD_COMMAND="scan-build-3.9"
   fi
   $SCAN_BUILD_COMMAND $CMAKE $CONFIGURE_FLAGS .

   # Put clang static analyzer results in scan/ and fail build if warnings found.
   SCAN_BUILD="$SCAN_BUILD_COMMAND -o scan --status-bugs"
else
   $CMAKE $CONFIGURE_FLAGS .
fi

$SCAN_BUILD make -j8 all

. $DIR/add-build-dirs-to-paths.sh
if [ -n "$SSL_VERSION" ]; then
   openssl version | grep -q $SSL_VERSION
fi
# This should fail when using fips capable OpenSSL when fips mode is enabled
openssl md5 README.rst || true

ulimit -c unlimited || true

if [ "$ANALYZE" != "ON" ]; then
   make -j8 install
fi

# We are done here if we don't want to run the tests.
if [ "$SKIP_MOCK_TESTS" = "ON" ]; then
   exit 0
fi

if [ "$VALGRIND" = "ON" ]; then
   # Defines "run_valgrind" shell function.
   . $DIR/valgrind.sh
else
   # Define a no-op function.
   run_valgrind ()
   {
      $@
   }
fi

export MONGOC_TEST_SERVER_LOG=stdout

# Write stderr to error.log and to console. Turn off tracing to avoid spurious
# log messages that CHECK_LOG considers failures.
mkfifo pipe || true
if [ -e pipe ]; then
   set +o xtrace
   tee error.log < pipe &
   run_valgrind ./src/libmongoc/test-libmongoc --no-fork -d -F test-results.json 2>pipe
   rm pipe
else
   run_valgrind ./src/libmongoc/test-libmongoc --no-fork -d -F test-results.json
fi

# Check if the error.log exists, and is more than 0 byte
if [ -s error.log ]; then
   cat error.log

   if [ "$CHECK_LOG" = "ON" ]; then
      # Ignore ar(1) warnings, and check the log again
      grep -v "^ar: " error.log > log.log
      if [ -s log.log ]; then
         cat error.log
         echo "Found unexpected error logs"
         # Mark build as failed if there is unknown things in the log
         exit 2
      fi
   fi
fi


if [ "$COVERAGE" = "ON" ]; then
   case "$CC" in
      clang)
         lcov --gcov-tool `pwd`/.evergreen/llvm-gcov.sh --capture --derive-func-data --directory . --output-file .coverage.lcov --no-external
         ;;
      *)
         lcov --gcov-tool gcov --capture --derive-func-data --directory . --output-file .coverage.lcov --no-external
         ;;
   esac
   genhtml .coverage.lcov --legend --title "mongoc code coverage" --output-directory coverage
fi
