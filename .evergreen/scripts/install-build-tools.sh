#!/usr/bin/env bash

export_uv_tool_dirs() {
  UV_TOOL_DIR="$(pwd)/uv-tool" || return
  UV_TOOL_BIN_DIR="$(pwd)/uv-bin" || return

  PATH="${UV_TOOL_BIN_DIR:?}:${PATH:-}"

  # Windows requires "C:\path\to\dir" instead of "/cygdrive/c/path/to/dir" (PATH is automatically converted).
  if [[ "${OSTYPE:?}" == cygwin ]]; then
    UV_TOOL_DIR="$(cygpath -aw "${UV_TOOL_DIR:?}")" || return
    UV_TOOL_BIN_DIR="$(cygpath -aw "${UV_TOOL_BIN_DIR:?}")" || return
  fi

  export UV_TOOL_DIR UV_TOOL_BIN_DIR
}

install_build_tools() {
  if [[ "${distro_id:?}" == "debian11-small" ]]; then
    # Temporary workaround for lack of uv on `debian11`. TODO: remove after DEVPROD-23011 is resolved.
    uv_dir="$(mktemp -d)"
    python3 -m virtualenv "${uv_dir:?}"
    # shellcheck source=/dev/null
    (. "${uv_dir:?}/bin/activate" && python -m pip install uv)
    PATH="${uv_dir:?}/bin:${PATH:-}"

    command -V uv # uv is hashed (/tmp/.../bin/uv)
    uv --version  # 0.9.5
  fi
  export_uv_tool_dirs || return

  uv tool install -q cmake || return

  if [[ -f /etc/redhat-release ]]; then
    # Avoid strange "Could NOT find Threads" CMake configuration error on RHEL when using PyPI CMake, PyPI Ninja, and
    # C++20 or newer by using MongoDB Toolchain's Ninja binary instead.
    ln -sf /opt/mongodbtoolchain/v4/bin/ninja "${UV_TOOL_BIN_DIR:?}/ninja" || return
  else
    uv tool install -q ninja || return
  fi

  cmake --version | head -n 1 || return
  echo "ninja version: $(ninja --version)" || return
}
