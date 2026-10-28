#!/usr/bin/env bash

declare script_dir
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

declare mongoc_dir
mongoc_dir="$(cd "${script_dir:?}/../.." && pwd)"

declare rust_toolchain_version
rust_toolchain_version="$(sed -n 's/^rust-version *= *"\([^"]*\)"/\1/p' "${mongoc_dir:?}/src/libmongoac/Cargo.toml")" || return
if [[ -z "${rust_toolchain_version}" ]]; then
  echo "Failed to read 'rust-version' from src/libmongoac/Cargo.toml" 1>&2
  return 1
fi
[[ "${rust_toolchain_version}" == *.*.* ]] || rust_toolchain_version+=".0"

declare rust_target="x86_64-unknown-linux-gnu"

# Install into the host-persistent cache directory when available (published as MONGO_C_DRIVER_CACHE_DIR by the
# `set-cache-dir` function) to avoid depending on (or clobbering) a pre-existing system or user installation. The
# checkout is wiped between Evergreen tasks, so installing there would re-download the toolchain on every run; local
# runs without the expansion fall back to the checkout, where the `.rust-toolchain` directory is gitignored. The
# install prefix is versioned so bumps do not conflict.
declare cache_dir="${MONGO_C_DRIVER_CACHE_DIR:-"${mongoc_dir:?}"}"
declare rust_toolchain_dir="${cache_dir:?}/.rust-toolchain"
declare install_prefix="${rust_toolchain_dir:?}/rust-${rust_toolchain_version:?}"
declare tarball_name="rust-${rust_toolchain_version:?}-${rust_target:?}.tar.xz"
declare tarball_path="${rust_toolchain_dir:?}/${tarball_name:?}"

export CARGO_HOME="${CARGO_HOME:-"${cache_dir:?}/.cargo"}"

# See: https://static.rust-lang.org/dist/${tarball_name:?}.sha256
declare rust_checksum="d2ccef59dd9f7439f2c694948069f789a044dc1addcc0803613232af8f88ee0c"

download_rust_toolchain() {
  declare tarball_tmp

  mkdir -p "${rust_toolchain_dir:?}" || return

  tarball_tmp="$(mktemp)" || return

  if curl --retry 5 --retry-connrefused --proto '=https' --tlsv1.2 --connect-timeout 10 --max-time 600 -sSfL \
    "https://static.rust-lang.org/dist/${tarball_name:?}" -o "${tarball_tmp}" &&
    mv "${tarball_tmp}" "${tarball_path}"; then
    return 0
  fi

  rm -f "${tarball_tmp}"
  echo "Failed to download the Rust toolchain from https://static.rust-lang.org/dist/${tarball_name:?}." 1>&2
  return 1
}

if [[ -x "${install_prefix:?}/bin/cargo" ]]; then
  echo "Reusing existing Rust toolchain at ${install_prefix:?}."
else
  echo "Installing Rust toolchain ${rust_toolchain_version:?} into ${install_prefix:?}..."

  [[ -f "${tarball_path:?}" ]] || download_rust_toolchain || return

  # Validate the checksum
  if ! echo "${rust_checksum:?}  ${tarball_path:?}" | sha256sum -c - >/dev/null; then
    rm -f "${tarball_path:?}"
    return 1
  fi

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

export PATH="${install_prefix:?}/bin:${PATH:-}"

echo "cargo version: $(cargo --version)"
echo "rustc version: $(rustc --version)"
