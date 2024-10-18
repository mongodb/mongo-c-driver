#!/usr/bin/env bash

compile_libmongocrypt() {
  declare -r cmake_binary="${1:?}"
  declare -r mongoc_dir="${2:?}"
  declare -r install_dir="${3:?}"

  # When updating libmongocrypt, consider also updating the copy of
  # libmongocrypt's kms-message in `src/kms-message`. Run
  # `.evergreen/scripts/kms-divergence-check.sh` to ensure that there is no
  # divergence in the copied files.

  # TODO: once 1.12.0 is released replace the following with:
  # git clone -q --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.12.0 || return
  {
    # TODO: update once https://github.com/mongodb/libmongocrypt/pull/895 is merged.
    git clone -q https://github.com/kevinAlbs/libmongocrypt || return
    # Check out commit removing use of (the now deprecated) bson_as_json
    git -C libmongocrypt checkout --branch CDRIVER-5700
  }

  declare -a crypt_cmake_flags=(
    "-DMONGOCRYPT_MONGOC_DIR=${mongoc_dir}"
    "-DBUILD_TESTING=OFF"
    "-DENABLE_ONLINE_TESTS=OFF"
    "-DENABLE_MONGOC=OFF"
    "-DBUILD_VERSION=1.11.0-pre"
  )

  env \
    DEBUG="0" \
    CMAKE_EXE="${cmake_binary}" \
    MONGOCRYPT_INSTALL_PREFIX=${install_dir} \
    DEFAULT_BUILD_ONLY=true \
    LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="${crypt_cmake_flags[*]}" \
    ./libmongocrypt/.evergreen/compile.sh || return
}

: "${1:?"missing path to CMake binary"}"
: "${2:?"missing path to mongoc directory"}"
: "${3:?"missing path to install directory"}"

compile_libmongocrypt "${1}" "${2}" "${3}"
