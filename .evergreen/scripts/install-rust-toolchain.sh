#!/usr/bin/env bash

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

# The pinned toolchain is defined in exactly one place: `channel` in src/libmongoac/rust-toolchain.toml (whose comment
# ties it to the MSRV declared by `rust-version` in src/libmongoac/Cargo.toml).
declare rust_toolchain_version
rust_toolchain_version="$(sed -n 's/^channel *= *"\([^"]*\)"/\1/p' "${mongoc_dir:?}/src/libmongoac/rust-toolchain.toml")" || return
if [[ -z "${rust_toolchain_version}" ]]; then
  echo "Failed to read 'channel' from src/libmongoac/rust-toolchain.toml" 1>&2
  return 1
fi

# Install rustup and cargo into the host-persistent cache directory when available (published as
# MONGO_C_DRIVER_CACHE_DIR by the `set-cache-dir` function) to avoid depending on (or clobbering) a pre-existing
# system or user installation. The checkout is wiped between Evergreen tasks, so installing there would
# re-download the toolchain on every run; local runs without the expansion fall back to the checkout. RHEL 8, for
# example, only provides Rust 1.79 via RPM, which is below the mongoac MSRV. Both are overridable via the
# environment.
declare cache_dir="${MONGO_C_DRIVER_CACHE_DIR:-"${mongoc_dir:?}"}"
export RUSTUP_HOME="${RUSTUP_HOME:-"${cache_dir}/.rustup"}"
export CARGO_HOME="${CARGO_HOME:-"${cache_dir}/.cargo"}"

install_rustup() {
  declare -i attempt
  declare -ri max_attempts=5
  declare rustup_init

  for ((attempt = 1; attempt <= max_attempts; attempt++)); do
    rustup_init="$(mktemp)" || return

    # Download the installer script before running it so that a truncated download is never executed. The protocol
    # and TLS version are pinned as recommended by rustup, and the timeouts ensure a hung connection cannot defeat
    # the retry loop below.
    if curl --retry 5 --retry-connrefused --proto '=https' --tlsv1.2 --connect-timeout 10 --max-time 300 -sSfL \
      "https://sh.rustup.rs" -o "${rustup_init}" &&
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
