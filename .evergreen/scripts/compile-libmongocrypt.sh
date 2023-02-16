#!/usr/bin/env bash

compile_libmongocrypt() {
  declare -r cmake_binary="${1:?}"
  declare -r mongoc_dir="${2:?}"
  declare -r install_dir="${3:?}"

  git clone -q --depth=1 https://github.com/mongodb/libmongocrypt --branch 1.7.0 || return

  # TODO: remove once latest libmongocrypt release contains commit c6f65fe6.
  {
    pushd libmongocrypt || return
    echo "1.7.0+c6f65fe6" >|VERSION_CURRENT
    git fetch -q origin master || return
    git checkout -q c6f65fe6 || return # Allows -DENABLE_MONGOC=OFF.
    popd || return                     # libmongocrypt
  }

  declare -a crypt_cmake_flags=(
    "-DMONGOCRYPT_MONGOC_DIR=${mongoc_dir}"
    "-DBUILD_TESTING=OFF"
    "-DENABLE_ONLINE_TESTS=OFF"
    "-DENABLE_MONGOC=OFF"
  )

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
