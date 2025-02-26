#!/usr/bin/env bash

set -o errexit
set -o pipefail

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

check_var_opt CC
check_var_opt CMAKE_GENERATOR
check_var_opt CMAKE_GENERATOR_PLATFORM

check_var_req C_STD_VERSION
check_var_opt CFLAGS
check_var_opt CXXFLAGS
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
configure_flags_append "-DENABLE_CLIENT_SIDE_ENCRYPTION=ON"
configure_flags_append "-DENABLE_DEBUG_ASSERTIONS=ON"
configure_flags_append "-DENABLE_MAINTAINER_FLAGS=ON"

if [[ "${C_STD_VERSION}" == "latest" ]]; then
  [[ "${CMAKE_GENERATOR:-}" =~ "Visual Studio" ]] || {
    echo "C_STD_VERSION=clatest is only supported with Visual Studio generators" 1>&2
    exit 1
  }

  configure_flags_append "-DCMAKE_C_FLAGS=/std:clatest"
else
  configure_flags_append_if_not_null C_STD_VERSION "-DCMAKE_C_STANDARD=${C_STD_VERSION}"
fi

if [[ "${OSTYPE}" == darwin* && "${HOSTTYPE}" == "arm64" ]]; then
  configure_flags_append "-DCMAKE_OSX_ARCHITECTURES=arm64"
fi

if [[ "${CMAKE_GENERATOR:-}" =~ "Visual Studio" ]]; then
  # Avoid C standard conformance issues with Windows 10 SDK headers.
  # See: https://developercommunity.visualstudio.com/t/stdc17-generates-warning-compiling-windowsh/1249671#T-N1257345
  configure_flags_append "-DCMAKE_SYSTEM_VERSION=10.0.20348.0"
fi

declare -a flags

if [[ "${CMAKE_GENERATOR:-}" =~ "Visual Studio" ]]; then
  # Even with -DCMAKE_SYSTEM_VERSION=10.0.20348.0, winbase.h emits conformance warnings.
  flags+=('/wd5105')
fi

if [[ "${OSTYPE}" == darwin* ]]; then
  flags+=('-Wno-unknown-pragmas')
fi

# CMake and compiler environment variables.
export CFLAGS
export CXXFLAGS

CFLAGS+=" ${flags+${flags[*]}}"
CXXFLAGS+=" ${flags+${flags[*]}}"

# Ensure find-cmake-latest.sh is sourced *before* add-build-dirs-to-paths.sh
# to avoid interfering with potential CMake build configuration.
# shellcheck source=.evergreen/scripts/find-cmake-latest.sh
. "${script_dir}/find-cmake-latest.sh"
declare cmake_binary
cmake_binary="$(find_cmake_latest)"

declare build_dir
build_dir="cmake-build"

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

export CMAKE_BUILD_PARALLEL_LEVEL
CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)"

if [[ "${CMAKE_GENERATOR:-}" =~ "Visual Studio" ]]; then
  # MSBuild needs additional assistance.
  # https://devblogs.microsoft.com/cppblog/improved-parallelism-in-msbuild/
  export UseMultiToolTask=1
  export EnforceProcessCountAcrossBuilds=1
fi

echo "Installing libmongocrypt..."
# shellcheck source=.evergreen/scripts/compile-libmongocrypt.sh
"${script_dir}/compile-libmongocrypt.sh" "${cmake_binary}" "${mongoc_dir}" "${install_dir}" &>output.txt || {
  cat output.txt 1>&2
  exit 1
}
echo "Installing libmongocrypt... done."

# Use ccache if able.
. "${script_dir:?}/find-ccache.sh"
find_ccache_and_export_vars "$(pwd)" || true
if command -v "${CMAKE_C_COMPILER_LAUNCHER:-}" && [[ "${OSTYPE:?}" == cygwin ]]; then
  configure_flags_append "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW"
  configure_flags_append "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
fi

echo "CFLAGS: ${CFLAGS}"
echo "configure_flags: ${configure_flags[*]}"

if [[ "${CMAKE_GENERATOR:-}" =~ "Visual Studio" ]]; then
  all_target="ALL_BUILD"
else
  all_target="all"
fi

"${cmake_binary}" -S . -B "${build_dir:?}" "${configure_flags[@]}"
"${cmake_binary}" --build "${build_dir:?}" --config Debug \
  --target mongo_c_driver_tests \
  --target mongo_c_driver_examples \
  --target public-header-warnings \
  --target "${all_target:?}"
