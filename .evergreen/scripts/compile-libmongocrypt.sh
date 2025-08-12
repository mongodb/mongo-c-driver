#!/usr/bin/env bash

compile_libmongocrypt() {
  declare -r cmake_binary="${1:?}"; shift
  declare -r mongoc_dir="${2:?}"; shift
  declare -r install_dir="${3:?}"; shift

  # When updating libmongocrypt, also update openssl-compat-check.sh and the copy of libmongocrypt's kms-message in
  # `src/kms-message`.
  #
  # Run `.evergreen/scripts/kms-divergence-check.sh` to ensure that there is no divergence in the copied files.
  declare -r version="1.13.0"

  git clone -q --depth=1 https://github.com/mongodb/libmongocrypt --branch "${version:?}" || return

  declare -a crypt_cmake_flags=(
    "-DMONGOCRYPT_MONGOC_DIR=${mongoc_dir}"
    "-DBUILD_TESTING=OFF"
    "-DENABLE_ONLINE_TESTS=OFF"
    "-DENABLE_MONGOC=OFF"
    "-DBUILD_VERSION=${version:?}"
  )

  . "$(dirname "${BASH_SOURCE[0]}")/find-ccache.sh"
  find_ccache_and_export_vars "$(pwd)/libmongocrypt" || true
  if command -v "${CMAKE_C_COMPILER_LAUNCHER:-}" && [[ "${OSTYPE:?}" == cygwin ]]; then
    crypt_cmake_flags+=(
      "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW"
      "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"
    )
  fi

  # Forward all extra arguments as extra CMake flags.
  crypt_cmake_flags+=($@)

  env \
    DEBUG="0" \
    CMAKE_EXE="${cmake_binary:?}" \
    MONGOCRYPT_INSTALL_PREFIX=${:?install_dir} \
    DEFAULT_BUILD_ONLY=true \
    LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="${crypt_cmake_flags[*]:?}" \
    ./libmongocrypt/.evergreen/compile.sh || return
}

: "${1:?"missing path to CMake binary"}"
: "${2:?"missing path to mongoc directory"}"
: "${3:?"missing path to install directory"}"

compile_libmongocrypt "${1}" "${2}" "${3}"
