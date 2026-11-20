#!/usr/bin/env bash

set -o errexit
set -o pipefail

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

declare install_dir="${MONGOAC_EXAMPLES_INSTALL_DIR:-${mongoc_dir:?}/install-dir}"
declare examples_dir="${mongoc_dir:?}/src/libmongoac/examples"
declare build_dir="${MONGOAC_EXAMPLES_BUILD_DIR:-${mongoc_dir:?}/_build/mongoac-examples}"

if [[ ! -d "${install_dir:?}" ]]; then
  echo "install directory not found: ${install_dir}" 1>&2
  echo "build and install mongoac first, e.g. ENABLE_MONGOAC=ON .evergreen/scripts/compile-mongoac.sh" 1>&2
  exit 1
fi

# shellcheck source=.evergreen/scripts/install-build-tools.sh
. "${script_dir:?}/install-build-tools.sh"
install_build_tools

export CMAKE_GENERATOR="Ninja"

cmake -S "${examples_dir:?}" -B "${build_dir:?}" \
  -D CMAKE_BUILD_TYPE=RelWithDebInfo \
  -D CMAKE_PREFIX_PATH="${install_dir:?}"
cmake --build "${build_dir:?}"

test -x "${build_dir:?}/queue-processing"
