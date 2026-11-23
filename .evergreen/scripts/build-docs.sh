#!/usr/bin/env bash

set -o errexit
set -o pipefail

. "$(dirname "${BASH_SOURCE[0]}")/env-var-utils.sh"
. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

check_var_opt CMAKE_GENERATOR "Ninja"
export CMAKE_GENERATOR

. "${MONGOC_DIR:?}/.evergreen/scripts/install-build-tools.sh"
install_build_tools

# Check that a CLion user didn't accidentally convert NEWS from UTF-8 to ASCII
grep "á" NEWS >/dev/null || (echo "NEWS file appears to have lost its UTF-8 encoding?" || exit 1)

build_dir="${MONGOC_DIR:?}/_build/for-docs"
cmake --fresh \
  -D ENABLE_MAN_PAGES=ON \
  -D ENABLE_HTML_DOCS=ON \
  -D ENABLE_ZLIB=BUNDLED \
  -B "${build_dir:?}" -S "${MONGOC_DIR:?}"
cmake --build "${build_dir:?}" --target bson-doc mongoc-doc
