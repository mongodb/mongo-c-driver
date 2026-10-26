#!/usr/bin/env bash

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

# Read the MSRV from the crate manifest, which is the authority (the "Rust Toolchain" requirement in
# src/libmongoac/doc/index.rst documents it). The prebuilt toolchain tarball is named with the full version, so a
# major.minor `rust-version` (e.g. "1.92") is normalized to the corresponding patch-zero release (e.g. "1.92.0").
declare rust_toolchain_version
rust_toolchain_version="$(sed -n 's/^rust-version *= *"\([^"]*\)"/\1/p' "${mongoc_dir:?}/src/libmongoac/Cargo.toml")" || return
if [[ -z "${rust_toolchain_version}" ]]; then
  echo "Failed to read 'rust-version' from src/libmongoac/Cargo.toml" 1>&2
  return 1
fi
[[ "${rust_toolchain_version}" == *.*.* ]] || rust_toolchain_version+=".0"

# The Rust target triple for the prebuilt toolchain. The mongoac Evergreen task runs on rhel8-latest (x86_64);
# parameterize this according to Rust's target triplet convention when other distros are added.
declare rust_target="x86_64-unknown-linux-gnu"

# Install into the host-persistent cache directory when available (published as MONGO_C_DRIVER_CACHE_DIR by the
# `set-cache-dir` function) to avoid depending on (or clobbering) a pre-existing system or user installation. The
# checkout is wiped between Evergreen tasks, so installing there would re-download the toolchain on every run; local
# runs without the expansion fall back to the checkout. The install prefix is versioned so bumps do not conflict.
declare cache_dir="${MONGO_C_DRIVER_CACHE_DIR:-"${mongoc_dir:?}"}"
declare install_prefix="${cache_dir:?}/rust-${rust_toolchain_version:?}"
declare tarball_name="rust-${rust_toolchain_version:?}-${rust_target:?}.tar.xz"
declare tarball_path="${cache_dir:?}/${tarball_name:?}"

# Keep the cargo registry cache between tasks on the same host.
export CARGO_HOME="${CARGO_HOME:-"${cache_dir:?}/.cargo"}"

# The published checksum of the tarball, which pins its contents. See:
# https://static.rust-lang.org/dist/${tarball_name:?}.sha256
declare rust_checksum="d2ccef59dd9f7439f2c694948069f789a044dc1addcc0803613232af8f88ee0c"

# Download the pinned toolchain tarball, with retries, and validate it against the checksum before it is extracted.
download_rust_toolchain() {
  declare -i attempt
  declare -ri max_attempts=5
  declare tarball_tmp

  for ((attempt = 1; attempt <= max_attempts; attempt++)); do
    tarball_tmp="$(mktemp)" || return

    # Download to a temporary file so that a truncated download is never extracted. The protocol and TLS version are
    # pinned, and the timeouts ensure a hung connection cannot defeat the retry loop below.
    if curl --retry 5 --retry-connrefused --proto '=https' --tlsv1.2 --connect-timeout 10 --max-time 600 -sSfL \
      "https://static.rust-lang.org/dist/${tarball_name:?}" -o "${tarball_tmp}" &&
      echo "${rust_checksum:?}  ${tarball_tmp}" | sha256sum -c - >/dev/null; then
      mv "${tarball_tmp}" "${tarball_path}" || return
      return 0
    fi

    rm -f "${tarball_tmp}"
    echo "Rust toolchain download attempt ${attempt}/${max_attempts} failed." 1>&2
    sleep 5
  done

  echo "Failed to download the Rust toolchain after ${max_attempts} attempts." 1>&2
  return 1
}

if [[ -x "${install_prefix:?}/bin/cargo" ]]; then
  echo "Reusing existing Rust toolchain at ${install_prefix:?}."
else
  echo "Installing Rust toolchain ${rust_toolchain_version:?} into ${install_prefix:?}..."

  [[ -f "${tarball_path:?}" ]] || download_rust_toolchain || return

  # Extract only what the build needs: cargo, rustc, and the standard library. `--strip-components=2` drops the
  # "rust-<version>-<target>/<component>/" prefix, yielding bin/, lib/, and share/ under the install prefix.
  # Extract into a temporary prefix and move it into place so an interrupted extraction cannot be reused.
  declare install_tmp="${install_prefix:?}.tmp"
  rm -rf "${install_tmp:?}" "${install_prefix:?}" || return
  mkdir -p "${install_tmp:?}" || return
  tar -xJf "${tarball_path:?}" --strip-components=2 -C "${install_tmp:?}" \
    "rust-${rust_toolchain_version:?}-${rust_target:?}/cargo" \
    "rust-${rust_toolchain_version:?}-${rust_target:?}/rustc" \
    "rust-${rust_toolchain_version:?}-${rust_target:?}/rust-std-${rust_target:?}" || return
  mv "${install_tmp:?}" "${install_prefix:?}" || return

  echo "Installing Rust toolchain ${rust_toolchain_version:?} into ${install_prefix:?}... done."
fi

# Ensure the toolchain is searched before any system-provided cargo or rustc. This is what makes CMake's
# `find_program (CARGO_EXECUTABLE cargo REQUIRED)` — executed later in the SAME process — find it.
export PATH="${install_prefix:?}/bin:${PATH:-}"

echo "cargo version: $(cargo --version)"
echo "rustc version: $(rustc --version)"
