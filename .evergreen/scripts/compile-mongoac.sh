#!/usr/bin/env bash

set -o errexit

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=.evergreen/scripts/install-rust-toolchain.sh
. "${script_dir:?}/install-rust-toolchain.sh"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

export EXTRA_LIB_PATH="${mongoc_dir:?}/cmake-build/src/libmongoac/Debug${EXTRA_LIB_PATH:+:${EXTRA_LIB_PATH}}"

export ENABLE_MONGOC=OFF
export ENABLE_MONGOAC=ON

# shellcheck source=.evergreen/scripts/compile.sh
"${script_dir:?}/compile.sh"
