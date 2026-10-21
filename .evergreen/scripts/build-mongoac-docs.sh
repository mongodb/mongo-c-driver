#!/usr/bin/env bash

set -o errexit # Exit the script with error if any of the commands fail

. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" paths

# Install and select a sufficiently recent CMake and Ninja, shadowing any older system-provided copies.
. "$MONGOC_DIR/.evergreen/scripts/install-build-tools.sh"
install_build_tools

# Install the Rust toolchain required by the mongoac library and put `cargo` on $PATH.
# shellcheck source=.evergreen/scripts/install-rust-toolchain.sh
. "$MONGOC_DIR/.evergreen/scripts/install-rust-toolchain.sh"

build_dir=$MONGOC_DIR/_build/for-mongoac-docs
uvx cmake --fresh -G "Ninja" -S "$MONGOC_DIR" -B "$build_dir" \
  -D ENABLE_MONGOAC=ON \
  -D ENABLE_HTML_DOCS=ON \
  -D ENABLE_MAN_PAGES=OFF \
  -D ENABLE_ZLIB=BUNDLED
uvx cmake --build "$build_dir" \
  --parallel 8 \
  --target mongoac-doc
