#!/usr/bin/env bash

set -o errexit
set -o pipefail

# Installs the Rust toolchain required to build the mongoac library (src/libmongoac) and exports the corresponding
# environment variables into the current shell.
#
# This script must be *sourced*, not executed, so that the exported environment (notably $PATH and
# $RUSTUP_TOOLCHAIN) is visible to subsequent commands in the same process, e.g. CMake's
# `find_program (CARGO_EXECUTABLE cargo REQUIRED)` in src/libmongoac/CMakeLists.txt.
#
# Usage:
#   . .evergreen/scripts/install-rust-toolchain.sh

# Keep synchronized with the MSRV declared by `rust-version` in src/libmongoac/Cargo.toml, the pin in
# src/libmongoac/rust-toolchain.toml, and the "Rust Toolchain" requirement in src/libmongoac/doc/index.rst.
declare rust_toolchain_version="1.88.0"

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

# Install rustup and cargo into the project directory to avoid depending on (or clobbering) a pre-existing system or
# user installation. RHEL 8, for example, only provides Rust 1.79 via RPM, which is below the mongoac MSRV.
export RUSTUP_HOME="${RUSTUP_HOME:-"${mongoc_dir:?}/.rustup"}"
export CARGO_HOME="${CARGO_HOME:-"${mongoc_dir:?}/.cargo"}"

install_rustup() {
  declare -i attempt
  declare -ri max_attempts=5
  declare rustup_init

  for ((attempt = 1; attempt <= max_attempts; attempt++)); do
    rustup_init="$(mktemp)" || return

    # Download the installer script before running it so that a truncated download is never executed.
    if curl --retry 5 --retry-connrefused -sSfL "https://sh.rustup.rs" -o "${rustup_init}" &&
      sh "${rustup_init}" -y --no-modify-path --profile minimal --default-toolchain "${rust_toolchain_version:?}"; then
      rm -f "${rustup_init}"
      return 0
    fi

    rm -f "${rustup_init}"
    echo "rustup installation attempt ${attempt}/${max_attempts} failed." 1>&2
    sleep 5
  done

  echo "Failed to install rustup after ${max_attempts} attempts." 1>&2
  return 1
}

if [[ -x "${CARGO_HOME:?}/bin/rustup" ]]; then
  echo "Reusing existing rustup installation at ${CARGO_HOME:?}."
else
  echo "Installing rustup into ${CARGO_HOME:?}..."
  install_rustup || return
  echo "Installing rustup into ${CARGO_HOME:?}... done."
fi

# Ensure $CARGO_HOME/bin is searched before any system-provided cargo or rustup.
export PATH="${CARGO_HOME:?}/bin:${PATH:-}"

# rustup resolves `rust-toolchain.toml` relative to the *current working directory*, which is the CMake binary
# directory for the cargo invocations issued by src/libmongoac/CMakeLists.txt. Select the toolchain explicitly so
# the MSRV is honored regardless of cwd. Overridable for convenience.
export RUSTUP_TOOLCHAIN="${RUSTUP_TOOLCHAIN:-"${rust_toolchain_version:?}"}"

# The default toolchain is already installed by rustup-init; this is a no-op unless it is missing, e.g. when reusing
# a pre-existing $CARGO_HOME.
rustup toolchain install "${rust_toolchain_version:?}" --profile minimal --no-self-update || return

echo "rustup version: $(rustup --version 2>&1 | head -n 1)"
echo "cargo version: $(cargo --version)"
echo "rustc version: $(rustc --version)"
echo "RUSTUP_TOOLCHAIN: ${RUSTUP_TOOLCHAIN:?}"
