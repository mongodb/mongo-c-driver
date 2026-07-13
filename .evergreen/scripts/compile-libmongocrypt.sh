#!/usr/bin/env bash

compile_libmongocrypt() {
  declare -r cmake_binary="${1:?"missing path to CMake binary"}"
  shift
  declare -r mongoc_dir="${1:?"missing path to mongoc directory"}"
  shift
  declare -r install_dir="${1:?"missing path to install directory"}"
  shift

  # When updating libmongocrypt, also update openssl-compat-check.sh and the copy of libmongocrypt's kms-message in
  # `src/kms-message`.
  #
  # Run `.evergreen/scripts/kms-divergence-check.sh` to ensure that there is no divergence in the copied files.
  declare -r version="1.21.0-dev"

  {
    git clone -q https://github.com/mongodb/libmongocrypt || return
    cd libmongocrypt || return
    # Check out libmongocrypt commit that pins to CMake 4.3.4 to work around MONGOCRYPT-952 until libmongocrypt updates the C driver.
    git checkout c2f80a7153da3537fe32916eef8ff1d4cc08bc67 || return
    cd .. || return
  }
  # TODO: after libmongocrypt is updated to use C driver 1.21.0, update the above to:
  # git clone --depth=1 -q https://github.com/mongodb/libmongocrypt --branch 1.21.0 || return


  git clone --depth=1 -q https://github.com/mongodb/libmongocrypt --branch "${version:?}" || return

  declare -a crypt_cmake_flags=(
    "-DMONGOCRYPT_MONGOC_DIR=${mongoc_dir}"
    "-DENABLE_ONLINE_TESTS=OFF"
    "-DENABLE_MONGOC=OFF"
    "-DBUILD_VERSION=${version:?}"
  )

  declare -a crypt_c_flags
  if [[ "${OSTYPE}" != "cygwin" ]]; then
    crypt_c_flags+=("-Wno-deprecated-declarations") # Remove after libmongocrypt upgrades to libbson 2.3.0+ (MONGOCRYPT-888) and migrates deprecated calls.
  fi

  . "$(dirname "${BASH_SOURCE[0]}")/find-ccache.sh"
  find_ccache_and_export_vars "$(pwd)/libmongocrypt" || true
  if command -v "${CMAKE_C_COMPILER_LAUNCHER:-}" && [[ "${OSTYPE:?}" == cygwin ]]; then
    crypt_cmake_flags+=(
      "-DCMAKE_POLICY_DEFAULT_CMP0141=NEW"
      "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"
    )
  fi

  # Forward all extra arguments as extra CMake flags.
  crypt_cmake_flags+=("$@")

  env \
    DEBUG="0" \
    CMAKE_EXE="${cmake_binary:?}" \
    MONGOCRYPT_INSTALL_PREFIX="${install_dir:?}" \
    DEFAULT_BUILD_ONLY="true" \
    BUILD_TESTING=OFF \
    LIBMONGOCRYPT_BUILD_VARIANTS=FALSE \
    LIBMONGOCRYPT_EXTRA_CMAKE_FLAGS="${crypt_cmake_flags[*]:?}" \
    LIBMONGOCRYPT_COMPILE_FLAGS="${crypt_c_flags[*]}" \
    ./libmongocrypt/.evergreen/compile.sh || return
}

compile_libmongocrypt "${@}"
