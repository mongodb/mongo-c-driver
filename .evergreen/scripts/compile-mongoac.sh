#!/usr/bin/env bash

# The Rust toolchain is installed into (and sourced from) the current shell so that CMake and its cargo
# invocations inherit the expected environment.
# shellcheck source=.evergreen/scripts/install-rust-toolchain.sh
. "$(dirname "${BASH_SOURCE[0]}")/install-rust-toolchain.sh"

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

# Unlike CMake targets, the mongoac shared library is produced by cargo and is therefore built without an RPATH or
# a SONAME (Linux) or install name (macOS). Executables that link against the build-tree copy (notably
# test-libmongoac, which Catch2's `catch_discover_tests` runs at build time) therefore require the build directory
# holding it to be on the library search path. `compile-unix.sh` builds with `CMAKE_BUILD_TYPE=Debug` unless
# RELEASE=ON, hence the "Debug" subdirectory.
# Consumed by `.evergreen/scripts/add-build-dirs-to-paths.sh`.
export EXTRA_LIB_PATH="${mongoc_dir:?}/cmake-build/src/libmongoac/Debug${EXTRA_LIB_PATH:+:${EXTRA_LIB_PATH}}"

export ENABLE_MONGOAC=ON

# shellcheck source=.evergreen/scripts/compile.sh
. "$(dirname "${BASH_SOURCE[0]}")/compile.sh"
