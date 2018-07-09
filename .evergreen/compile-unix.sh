#!/bin/sh
set -o xtrace   # Write all commands first to stderr
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
#       SKIP_TESTS              Skips running the libmongoc tests after compiling
# Options for CMake:
#       LIBBSON                 Build against bundled or external libbson
#       EXTRA_CONFIGURE_FLAGS   Extra configure flags to use
#       ZLIB                    Build against bundled or external zlib, or none
#       SNAPPY                  Build against bundled or external Snappy, or none
#       SSL                     Build against OpenSSL or native or none
#       SASL                    Build against SASL or not
#       ENABLE_SHM_COUNTERS     Build with SHM counters

# Options for this script.
RELEASE=${RELEASE:-OFF}
DEBUG=${DEBUG:-OFF}
TRACING=${TRACING:-OFF}
VALGRIND=${VALGRIND:-OFF}
ANALYZE=${ANALYZE:-OFF}
COVERAGE=${COVERAGE:-OFF}
RDTSCP=${RDTSCP:-OFF}
SKIP_TESTS=${SKIP_TESTS:-OFF}
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
echo "SKIP_TESTS: $SKIP_TESTS"
echo "ZLIB: $ZLIB"

# Get the kernel name, lowercased
OS=$(uname -s | tr '[:upper:]' '[:lower:]')
echo "OS: $OS"

# Automatically retrieve the machine architecture, lowercase, unless provided
# as an environment variable (e.g. to force 32bit)
[ -z "$MARCH" ] && MARCH=$(uname -m | tr '[:upper:]' '[:lower:]')

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
   -DCMAKE_PREFIX_PATH=$INSTALL_DIR \
   -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
   -DENABLE_SHM_COUNTERS=$ENABLE_SHM_COUNTERS \
"

if [ ! -z "$ZLIB" ]; then
   DEBUG_AND_RELEASE_FLAGS="$DEBUG_AND_RELEASE_FLAGS -DENABLE_ZLIB=${ZLIB}"
fi

if [ ! -z "$SNAPPY" ]; then
   DEBUG_AND_RELEASE_FLAGS="$DEBUG_AND_RELEASE_FLAGS -DENABLE_SNAPPY=${SNAPPY}"
fi

DEBUG_FLAGS="${DEBUG_AND_RELEASE_FLAGS} -DCMAKE_BUILD_TYPE=Debug"
RELEASE_FLAGS="${DEBUG_AND_RELEASE_FLAGS} -DCMAKE_BUILD_TYPE=Release"

DIR=$(dirname $0)
. $DIR/find-cmake.sh
. $DIR/set-path.sh

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
CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SSL=${SSL}"
[ "$COVERAGE" = "ON" ] && CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_COVERAGE=ON -DENABLE_EXAMPLES=OFF"

if [ "$RELEASE" = "ON" ]; then
   # Build from the release tarball.
   mkdir build-dir
   $TAR xf ../mongoc.tar.gz -C build-dir --strip-components=1
   cd build-dir
fi

# UndefinedBehaviorSanitizer configuration
export UBSAN_OPTIONS="print_stacktrace=1 abort_on_error=1"
# AddressSanitizer configuration
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1"

case "$MARCH" in
   i386)
      CFLAGS="$CFLAGS -m32 -march=i386"
      CXXFLAGS="$CXXFLAGS -m32 -march=i386"
      CONFIGURE_FLAGS="$CONFIGURE_FLAGS -DENABLE_SNAPPY=AUTO -DENABLE_ZLIB=BUNDLED"
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

if [ "$ANALYZE" = "ON" ]; then
   # Clang static analyzer, available on Ubuntu 16.04 images.
   # https://clang-analyzer.llvm.org/scan-build.html
   scan-build $CMAKE $CONFIGURE_FLAGS

   # Put clang static analyzer results in scan/ and fail build if warnings found.
   SCAN_BUILD="scan-build -o scan --status-bugs"
else
   $CMAKE $CONFIGURE_FLAGS
fi

openssl version
if [ -n "$SSL_VERSION" ]; then
   openssl version | grep -q $SSL_VERSION
fi
# This should fail when using fips capable OpenSSL when fips mode is enabled
openssl md5 README.rst || true
$SCAN_BUILD make -j8 all

ulimit -c unlimited || true


# We are done here if we don't want to run the tests.
if [ "$SKIP_TESTS" = "ON" ]; then
   exit 0
fi

# Write stderr to error.log and to console.
# TODO: valgrind
mkfifo pipe || true
if [ -e pipe ]; then
   tee error.log < pipe &
   ./src/libmongoc/test-libmongoc -d -F test-results.json 2>pipe
   rm pipe
else
   ./src/libmongoc/test-libmongoc -d -F test-results.json
fi

# Check if the error.log exists, and is more than 0 byte
if [ -s error.log ]; then
   cat error.log

   if [ "$CHECK_LOG" = "ON" ]; then
      # Ignore ar(1) warnings, and check the log again
      grep -v "^ar: " error.log > log.log
      if [ -s log.log ]; then
         cat error.log
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
