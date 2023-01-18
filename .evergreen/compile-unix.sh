#!/usr/bin/env bash

set -o errexit
set -o pipefail

to_absolute() (
  cd "${1:?}" && pwd
)

print_var() {
  printf "%s: %s\n" "${1:?}" "${!1:-}"
}

check_var_opt() {
  printf -v "${1:?}" "%s" "${!1:-"${2:-}"}"
  print_var "${1}"
}

check_var_opt "ANALYZE" "OFF"
check_var_opt "C_STD_VERSION" # CMake default: 99.
check_var_opt "CC"
check_var_opt "CFLAGS"
check_var_opt "CHECK_LOG" "OFF"
check_var_opt "COMPILE_LIBMONGOCRYPT" "OFF"
check_var_opt "COVERAGE" # CMake default: OFF.
check_var_opt "CXXFLAGS"
check_var_opt "DEBUG" "OFF"
check_var_opt "ENABLE_SHM_COUNTERS" # CMake default: AUTO.
check_var_opt "EXTRA_CMAKE_PREFIX_PATH"
check_var_opt "EXTRA_CONFIGURE_FLAGS"
check_var_opt "ENABLE_RDTSCP" "OFF"
check_var_opt "RELEASE" "OFF"
check_var_opt "SANITIZE"
check_var_opt "SASL" "OFF" # CMake default: AUTO.
check_var_opt "SKIP_MOCK_TESTS" "OFF"
check_var_opt "SNAPPY"         # CMake default: AUTO.
check_var_opt "SRV"            # CMake default: AUTO.
check_var_opt "SSL" "OFF"      # CMake default: AUTO.
check_var_opt "TRACING"        # CMake default: OFF.
check_var_opt "ZLIB" "BUNDLED" # CMake default: AUTO.
check_var_opt "ZSTD"           # CMake default: AUTO.

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/..")"

declare install_dir="${mongoc_dir}/install-dir"

declare cmake_prefix_path="${install_dir}"
if [[ -n "${EXTRA_CMAKE_PREFIX_PATH}" ]]; then
  cmake_prefix_path+=";${EXTRA_CMAKE_PREFIX_PATH}"
fi

declare -a configure_flags

configure_flags_append() {
  configure_flags+=("${@:?}")
}

configure_flags_append_if_not_null() {
  if [[ -n "${1?}" ]]; then
    shift
    configure_flags+=("${@:?}")
  fi
}

configure_flags_append "-DCMAKE_INSTALL_PREFIX=${install_dir}"
configure_flags_append "-DCMAKE_PREFIX_PATH=${cmake_prefix_path}"
configure_flags_append "-DCMAKE_SKIP_RPATH=TRUE" # Avoid hardcoding absolute paths to dependency libraries.
configure_flags_append "-DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF"
configure_flags_append "-DENABLE_BSON=ON"
configure_flags_append "-DENABLE_HTML_DOCS=OFF"
configure_flags_append "-DENABLE_MAINTAINER_FLAGS=ON"
configure_flags_append "-DENABLE_MAN_PAGES=OFF"

configure_flags_append_if_not_null "${C_STD_VERSION}" "-DCMAKE_C_STANDARD=${C_STD_VERSION}"
configure_flags_append_if_not_null "${ENABLE_RDTSCP}" "-DENABLE_RDTSCP=${ENABLE_RDTSCP}"
configure_flags_append_if_not_null "${ENABLE_SHM_COUNTERS}" "-DENABLE_SHM_COUNTERS=${ENABLE_SHM_COUNTERS}"
configure_flags_append_if_not_null "${SANITIZE}" "-DMONGO_SANITIZE=${SANITIZE}"
configure_flags_append_if_not_null "${SASL}" "-DENABLE_SASL=${SASL}"
configure_flags_append_if_not_null "${SNAPPY}" "-DENABLE_SNAPPY=${SNAPPY}"
configure_flags_append_if_not_null "${SRV}" "-DENABLE_SRV=${SRV}"
configure_flags_append_if_not_null "${TRACING}" "-DENABLE_TRACING=${TRACING}"
configure_flags_append_if_not_null "${ZLIB}" "-DENABLE_ZLIB=${ZLIB}"

if [[ "${DEBUG}" == "ON" ]]; then
  configure_flags_append "-DCMAKE_BUILD_TYPE=Debug"
else
  configure_flags_append "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
fi

if [[ "${SSL}" == "OPENSSL_STATIC" ]]; then
  configure_flags_append "-DENABLE_SSL=OPENSSL" "-DOPENSSL_USE_STATIC_LIBS=ON"
else
  configure_flags_append_if_not_null "${SSL}" "-DENABLE_SSL=${SSL}"
fi

if [[ "${COVERAGE}" == "ON" ]]; then
  configure_flags_append "-DENABLE_COVERAGE=ON" "-DENABLE_EXAMPLES=OFF"
fi

if [[ "${RELEASE}" == "ON" ]]; then
  # Build from the release tarball.
  mkdir build-dir
  declare tar_args="xf ../mongoc.tar.gz -C build-dir --strip-components=1"

  # --strip-components is an GNU tar extension. Check if the platform
  # has GNU tar installed as `gtar`, otherwise we assume to be on
  # platform that supports it
  # command -v returns success error code if found and prints the path to it
  if command -v gtar 2>/dev/null; then
    # shellcheck disable=SC2086
    gtar ${tar_args}
  else
    # shellcheck disable=SC2086
    tar ${tar_args}
  fi

  cd build-dir
else
  configure_flags_append "-DENABLE_DEBUG_ASSERTIONS=ON"
fi

if [[ -n "${ZSTD}" ]]; then
  # Since zstd inconsitently installed on macos-1014.
  # Remove this check in CDRIVER-3483.
  if [[ "${OSTYPE}" != darwin* ]]; then
    configure_flags_append "-DENABLE_ZSTD=${ZSTD}"
  fi
fi

declare flags

case "${HOSTTYPE}" in
s390x)
  flags="-march=z196 -mtune=zEC12"
  ;;
x86_64)
  flags="-m64 -march=x86-64"
  ;;
powerpc64le)
  flags="-mcpu=power8 -mtune=power8 -mcmodel=medium"
  ;;
*)
  flags=""
  ;;
esac

# CMake and compiler environment variables.
export CC
export CXX
export CFLAGS
export CXXFLAGS

CFLAGS+=" ${flags}"
CXXFLAGS+=" ${flags}"

if [[ "${OSTYPE}" == darwin* ]]; then
  CFLAGS+=" -Wno-unknown-pragmas"
fi

case "${CC}" in
clang)
  CXX=clang++
  ;;
gcc)
  CXX=g++
  ;;
esac

if [[ "${OSTYPE}" == darwin* && "${HOSTTYPE}" == "arm64" ]]; then
  configure_flags_append "-DCMAKE_OSX_ARCHITECTURES=arm64"
fi

# Ensure find-cmake.sh is sourced *before* add-build-dirs-to-paths.sh
# to avoid interfering with potential CMake build configuration.
# shellcheck source=.evergreen/find-cmake.sh
. "${script_dir}/find-cmake.sh" # ${CMAKE}

# shellcheck source=.evergreen/add-build-dirs-to-paths.sh
. "${script_dir}/add-build-dirs-to-paths.sh"

export PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH}"

echo "SSL Version: $(pkg-config --modversion libssl || echo "N/A")"

if [[ "${OSTYPE}" == darwin* ]]; then
  # llvm-cov is installed from brew.
  # Add path to scan-build and lcov, but with low priority to avoid overwriting
  # paths to other system-installed LLVM binaries such as clang.
  PATH="${PATH}:/usr/local/opt/llvm/bin"
fi

if [[ "${COMPILE_LIBMONGOCRYPT}" == "ON" ]]; then
  # Build libmongocrypt, using the previously fetched installed source.
  # TODO(CDRIVER-4394) update to use libmongocrypt 1.7.0 once there is a stable 1.7.0 release.
  git clone --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.7.0-alpha1

  mkdir libmongocrypt/cmake-build
  pushd libmongocrypt/cmake-build
  "${CMAKE}" \
    -DENABLE_SHARED_BSON=ON \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DMONGOCRYPT_MONGOC_DIR="${mongoc_dir}" \
    -DCMAKE_INSTALL_PREFIX="${install_dir}" \
    -DCMAKE_PREFIX_PATH="${cmake_prefix_path}" \
    -DBUILD_TESTING=OFF \
    ..
  make -j install
  popd # libmongocrypt/cmake-build
else
  # Hosts may have libmongocrypt installed from apt/yum. We do not want to pick those up
  # since those libmongocrypt packages statically link libbson.
  # Note: may be overwritten by ${EXTRA_CONFIGURE_FLAGS}.
  configure_flags_append "-DENABLE_CLIENT_SIDE_ENCRYPTION=OFF"
fi

if [[ "${ANALYZE}" == "ON" ]]; then
  # Clang static analyzer, available on Ubuntu 16.04 images.
  # https://clang-analyzer.llvm.org/scan-build.html
  #
  # On images other than Ubuntu 16.04, use scan-build-3.9 if
  # scan-build is not found.
  declare scan_build_binary
  if command -v scan-build 2>/dev/null; then
    scan_build_binary="scan-build"
  else
    scan_build_binary="scan-build-3.9"
  fi

  # Do not include bundled zlib in scan-build analysis.
  # scan-build `--exclude`` flag is not available on all Evergreen variants.
  configure_flags_append "-DENABLE_ZLIB=OFF"

  # shellcheck disable=SC2086
  "${scan_build_binary}" "${CMAKE}" "${configure_flags[@]}" ${EXTRA_CONFIGURE_FLAGS} .

  # Put clang static analyzer results in scan/ and fail build if warnings found.
  "${scan_build_binary}" -o scan --status-bugs make -j all
else
  # shellcheck disable=SC2086
  "${CMAKE}" "${configure_flags[@]}" ${EXTRA_CONFIGURE_FLAGS} .
  make -j all
fi

# shellcheck source=.evergreen/bypass-dlclose.sh
. "${script_dir}/bypass-dlclose.sh"

# This should fail when using fips capable OpenSSL when fips mode is enabled
openssl md5 README.rst || true

ulimit -c unlimited || true

if [[ "${ANALYZE}" != "ON" ]]; then
  make install
fi

# We are done here if we don't want to run the tests.
if [[ "${SKIP_MOCK_TESTS}" == "ON" ]]; then
  exit 0
fi

# Sanitizer environment variables.
export ASAN_OPTIONS="detect_leaks=1 abort_on_error=1 symbolize=1"
export ASAN_SYMBOLIZER_PATH="/opt/mongodbtoolchain/v3/bin/llvm-symbolizer"
export TSAN_OPTIONS="suppressions=./.tsan-suppressions"
export UBSAN_OPTIONS="print_stacktrace=1 abort_on_error=1"

# C Driver test environment variables.
export MONGOC_TEST_FUTURE_TIMEOUT_MS=30000
export MONGOC_TEST_IPV4_AND_IPV6_HOST="ipv4_and_ipv6.test.build.10gen.cc"
export MONGOC_TEST_SERVER_LOG=stdout
export MONGOC_TEST_SKIP_LIVE=on
export MONGOC_TEST_SKIP_SLOW=on

declare -a test_args=(
  "--no-fork"
  "-d"
  "-F"
  "test-results.json"
  "--skip-tests"
  "${script_dir}/skip-tests.txt"
)

declare ld_preload="${LD_PRELOAD:-}"
if [[ "${SANITIZE}" =~ address ]]; then
  ld_preload="$(bypass_dlclose):${ld_preload:-}"
fi

# Write stderr to error.log and to console. Turn off tracing to avoid spurious
# log messages that CHECK_LOG considers failures.
mkfifo pipe || true
if [[ -e pipe ]]; then
  set +o xtrace
  tee error.log <pipe &
  LD_PRELOAD="${ld_preload:-}" ./src/libmongoc/test-libmongoc "${test_args[@]}" 2>pipe
  rm pipe
else
  LD_PRELOAD="${ld_preload:-}" ./src/libmongoc/test-libmongoc "${test_args[@]}"
fi

# Check if the error.log exists, and is more than 0 byte
if [[ -s error.log ]]; then
  cat error.log

  if [[ "${CHECK_LOG}" == "ON" ]]; then
    # Ignore ar(1) warnings, and check the log again
    grep -v "^ar: " error.log >log.log
    if [[ -s log.log ]]; then
      cat error.log
      echo "Found unexpected error logs" 1>&2
      # Mark build as failed if there is unknown things in the log
      exit 2
    fi
  fi
fi

if [[ "${COVERAGE}" == "ON" ]]; then
  declare -a coverage_args=(
    "--capture"
    "--derive-func-data"
    "--directory"
    "."
    "--output-file"
    ".coverage.lcov"
    "--no-external"
  )

  case "${CC}" in
  clang)
    lcov --gcov-tool "$(pwd)/.evergreen/llvm-gcov.sh" "${coverage_args[@]}"
    ;;
  *)
    lcov --gcov-tool gcov "${coverage_args[@]}"
    ;;
  esac

  genhtml .coverage.lcov --legend --title "mongoc code coverage" --output-directory coverage
fi
