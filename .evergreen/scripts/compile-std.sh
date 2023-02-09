#!/usr/bin/env bash

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"

check_var_opt C_STD_VERSION
check_var_opt CC
check_var_opt CFLAGS
check_var_opt MARCH

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/../..")"

declare install_dir="${mongoc_dir}/install-dir"

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

configure_flags_append "-DCMAKE_PREFIX_PATH=${install_dir}"
configure_flags_append "-DCMAKE_SKIP_RPATH=TRUE" # Avoid hardcoding absolute paths to dependency libraries.
configure_flags_append "-DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF"
configure_flags_append "-DENABLE_DEBUG_ASSERTIONS=ON"
configure_flags_append "-DENABLE_MAINTAINER_FLAGS=ON"

configure_flags_append_if_not_null C_STD_VERSION "-DCMAKE_C_STANDARD=${C_STD_VERSION}"

declare -a flags

case "${MARCH}" in
i686)
  flags+=("-m32" "-march=i386")
  ;;
esac

case "${HOSTTYPE}" in
s390x)
  flags+=("-march=z196" "-mtune=zEC12")
  ;;
x86_64)
  flags+=("-m64" "-march=x86-64")
  ;;
powerpc64le)
  flags+=("-mcpu=power8" "-mtune=power8" "-mcmodel=medium")
  ;;
esac

# CMake and compiler environment variables.
export CC
export CXX
export CFLAGS
export CXXFLAGS

CFLAGS+=" ${flags[*]}"
CXXFLAGS+=" ${flags[*]}"

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
# shellcheck source=.evergreen/scripts/find-cmake.sh
. "${script_dir}/find-cmake.sh" # ${CMAKE}

# shellcheck source=.evergreen/scripts/add-build-dirs-to-paths.sh
. "${script_dir}/add-build-dirs-to-paths.sh"

export PKG_CONFIG_PATH
PKG_CONFIG_PATH="${install_dir}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

if [[ "${OSTYPE}" == darwin* ]]; then
  # MacOS does not have nproc.
  nproc() {
    sysctl -n hw.logicalcpu
  }
fi

git clone -q --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.7.0
# TODO: remove once latest libmongocrypt release contains commit 4c4aa8bf.
{
  pushd libmongocrypt
  echo "1.7.0+4c4aa8bf" >|VERSION_CURRENT
  git fetch -q origin master
  git checkout -q 4c4aa8bf # Allows -DENABLE_MONGOC=OFF.
  popd                     # libmongocrypt
}
declare -a crypt_cmake_flags
crypt_cmake_flags=(
  "-DMONGOCRYPT_MONGOC_DIR=${mongoc_dir}"
  "-DBUILD_TESTING=OFF"
  "-DENABLE_ONLINE_TESTS=OFF"
  "-DENABLE_MONGOC=OFF"
)
MONGOCRYPT_INSTALL_PREFIX=${install_dir} \
  DEFAULT_BUILD_ONLY=true \
  LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="${crypt_cmake_flags[*]}" \
  ./libmongocrypt/.evergreen/compile.sh
# Fail if the C driver is unable to find the installed libmongocrypt.
configure_flags_append "-DENABLE_CLIENT_SIDE_ENCRYPTION=ON"

echo "CFLAGS: ${CFLAGS}"
echo "configure_flags: ${configure_flags[*]}"

"${CMAKE}" "${configure_flags[@]}" .
make -j "$(nproc)" all
