#!/usr/bin/env bash

set -o errexit

# shellcheck source=.evergreen/scripts/env-var-utils.sh
. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

check_var_opt CMAKE_GENERATOR "Ninja"
export CMAKE_GENERATOR

. "${MONGOC_DIR:?}/.evergreen/scripts/install-build-tools.sh"
install_build_tools

. "${MONGOC_DIR:?}/.evergreen/scripts/install-rust-toolchain.sh"

build_dir=${MONGOC_DIR:?}/_build/for-mongoac-docs
cmake --fresh -S "${MONGOC_DIR:?}" -B "${build_dir:?}" \
  -D ENABLE_MONGOAC=ON \
  -D ENABLE_HTML_DOCS=ON \
  -D ENABLE_MAN_PAGES=ON \
  -D ENABLE_ZLIB=BUNDLED
cmake --build "${build_dir:?}" --target mongoac-doc
