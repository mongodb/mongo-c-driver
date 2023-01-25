#!/usr/bin/env bash

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"

check_var_opt ANALYZE "OFF"
check_var_opt C_STD_VERSION # CMake default: 99.
check_var_opt CC
check_var_opt CFLAGS
check_var_opt CHECK_LOG "OFF"
check_var_opt COMPILE_LIBMONGOCRYPT "OFF"
check_var_opt COVERAGE # CMake default: OFF.
check_var_opt CXXFLAGS
check_var_opt DEBUG "OFF"
check_var_opt ENABLE_SHM_COUNTERS # CMake default: AUTO.
check_var_opt EXTRA_CMAKE_PREFIX_PATH
check_var_opt EXTRA_CONFIGURE_FLAGS
check_var_opt ENABLE_RDTSCP "OFF"
check_var_opt RELEASE "OFF"
check_var_opt SANITIZE
check_var_opt SASL "OFF"     # CMake default: AUTO.
check_var_opt SNAPPY         # CMake default: AUTO.
check_var_opt SRV            # CMake default: AUTO.
check_var_opt SSL "OFF"      # CMake default: AUTO.
check_var_opt TRACING        # CMake default: OFF.
check_var_opt ZLIB "BUNDLED" # CMake default: AUTO.
check_var_opt ZSTD           # CMake default: AUTO.

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/..")"

declare install_dir="${mongoc_dir}/install-dir"
declare openssl_install_dir="${mongoc_dir}/openssl-install-dir"

declare cmake_prefix_path="${install_dir}"
if [[ -n "${EXTRA_CMAKE_PREFIX_PATH}" ]]; then
  cmake_prefix_path+=";${EXTRA_CMAKE_PREFIX_PATH}"
fi

declare -a configure_flags

configure_flags_append() {
  configure_flags+=("${@:?}")
}

configure_flags_append_if_not_null() {
  declare var="${1:?}"
  if [[ -n "${!var:-}" ]]; then
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

configure_flags_append_if_not_null C_STD_VERSION "-DCMAKE_C_STANDARD=${C_STD_VERSION}"
configure_flags_append_if_not_null ENABLE_RDTSCP "-DENABLE_RDTSCP=${ENABLE_RDTSCP}"
configure_flags_append_if_not_null ENABLE_SHM_COUNTERS "-DENABLE_SHM_COUNTERS=${ENABLE_SHM_COUNTERS}"
configure_flags_append_if_not_null SANITIZE "-DMONGO_SANITIZE=${SANITIZE}"
configure_flags_append_if_not_null SASL "-DENABLE_SASL=${SASL}"
configure_flags_append_if_not_null SNAPPY "-DENABLE_SNAPPY=${SNAPPY}"
configure_flags_append_if_not_null SRV "-DENABLE_SRV=${SRV}"
configure_flags_append_if_not_null TRACING "-DENABLE_TRACING=${TRACING}"
configure_flags_append_if_not_null ZLIB "-DENABLE_ZLIB=${ZLIB}"

if [[ "${DEBUG}" == "ON" ]]; then
  configure_flags_append "-DCMAKE_BUILD_TYPE=Debug"
else
  configure_flags_append "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
fi

if [[ "${SSL}" == "OPENSSL_STATIC" ]]; then
  configure_flags_append "-DENABLE_SSL=OPENSSL" "-DOPENSSL_USE_STATIC_LIBS=ON"
else
  configure_flags_append_if_not_null SSL "-DENABLE_SSL=${SSL}"
fi

if [[ "${COVERAGE}" == "ON" ]]; then
  configure_flags_append "-DENABLE_COVERAGE=ON" "-DENABLE_EXAMPLES=OFF"
fi

if [[ "${RELEASE}" == "ON" ]]; then
  # Build from the release tarball.
  mkdir build-dir
  declare -a tar_args=("xf" "../mongoc.tar.gz" "-C" "build-dir" "--strip-components=1")

  # --strip-components is an GNU tar extension. Check if the platform
  # has GNU tar installed as `gtar`, otherwise we assume to be on
  # platform that supports it
  # command -v returns success error code if found and prints the path to it
  if command -v gtar 2>/dev/null; then
    gtar "${tar_args[@]}"
  else
    tar "${tar_args[@]}"
  fi

  cd build-dir
else
  configure_flags_append "-DENABLE_DEBUG_ASSERTIONS=ON"
fi

if [[ -n "${ZSTD}" ]]; then
  # Since zstd is inconsistently installed on macos-1014.
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

export PKG_CONFIG_PATH
PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

if [[ -d "${openssl_install_dir}" ]]; then
  # Use custom SSL library if present.
  configure_flags_append "-DOPENSSL_ROOT_DIR=${openssl_install_dir}"
  PKG_CONFIG_PATH="${openssl_install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

echo "SSL Version: $(pkg-config --modversion libssl 2>/dev/null || echo "N/A")"

if [[ "${OSTYPE}" == darwin* ]]; then
  # llvm-cov is installed from brew.
  # Add path to scan-build and lcov, but with low priority to avoid overwriting
  # paths to other system-installed LLVM binaries such as clang.
  PATH="${PATH}:/usr/local/opt/llvm/bin"

  # MacOS does not have nproc.
  nproc() {
    sysctl -n hw.logicalcpu
  }
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
  make -j "$(nproc)" install
  popd # libmongocrypt/cmake-build
else
  # Avoid symbol collisions with libmongocrypt installed via apt/yum.
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
  "${scan_build_binary}" -o scan --status-bugs make -j "$(nproc)" all
else
  # shellcheck disable=SC2086
  "${CMAKE}" "${configure_flags[@]}" ${EXTRA_CONFIGURE_FLAGS} .
  make -j "$(nproc)" all
  make install
fi
