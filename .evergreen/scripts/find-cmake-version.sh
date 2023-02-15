#!/usr/bin/env bash

# Used to workaround curl certificate verification failures on certain distros.
: "${distro_id:?"missing required Evergreen expansion"}"

# Create a temporary directory in the existing directory $1.
make_tmpdir_in() {
  declare -r target_dir="${1:?"missing path to target directory"}"

  if [[ ! -d "${target_dir}" ]]; then
    echo "make_tmpdir_in: target directory ${target_dir} does not exist!" 1>&2
    return 1
  fi

  declare tmpdir

  {
    if [[ "${OSTYPE:?}" == darwin* ]]; then
      # mktemp on MacOS does not support -p.
      tmpdir="$(mktemp -d)" || return
      declare tmpbase
      tmpbase="$(basename "${tmpdir}")" || return
      mv -f "${tmpdir}" "${target_dir}" || return
      tmpdir="${target_dir}/${tmpbase}" || return
    else
      tmpdir="$(mktemp -d -p "${target_dir}")" || return
    fi
  } 1>&2

  printf "%s" "${tmpdir}"
}

# Ensure "good enough" atomicity when two or more tasks running on the same host
# attempt to install a version of CMake.
cmake_replace_latest() {
  declare -r cache="${1:?"missing path to cache directory"}"
  declare -r root="${2:?"missing path to latest CMake root directory"}"
  declare -r version="${3:?"missing latest CMake version"}"
  declare -r cmake_cache="${cache}/cmake-${version}"

  # Doesn't matter who creates the cache directory.
  mkdir -p "${cache}" || return

  # Should not happen, but handle it anyways.
  [[ -L "${cmake_cache}" ]] || rm -rf "${cmake_cache}" || return

  # Temporary directory to create new symlink.
  declare tmpdir
  tmpdir="$(make_tmpdir_in "${cache}")" || return

  # Symlink to the new CMake root directory.
  ln -s "${root}" "${tmpdir}/cmake-${version}" || return

  # Atomic substitution of symlink from old to new CMake root directory.
  mv -f "${tmpdir}/cmake-${version}" "${cache}" || return

  # Be nice and clean up the now-empty temporary directory.
  rmdir "${tmpdir}" || return
}

# find_cmake_version
#
# Usage:
#   find_cmake_version 3 1 0
#   cmake_binary="$(find_cmake_version 3 1 0)"
#   cmake_binary="$(find_cmake_version 3 25 0 2>/dev/null)"
#
# Return 0 (true) if a CMake binary matching the requested version is available.
# Return a non-zero (false) value otherwise.
#
# If successful, print the name of the CMake binary to stdout (pipe 1).
# Otherwise, no output is printed to stdout (pipe 1).
#
# Diagnostic messages may be printed to stderr (pipe 2). Redirect to /dev/null
# with `2>/dev/null` to silence these messages.
#
# Example:
#   cmake_binary="$(find_cmake_version 3 25 0)" || exit
#   "${cmake_binary:?}" -S path-to-source -B path-to-build
find_cmake_version() {
  declare -r major="${1:?"missing CMake major version"}"
  declare -r minor="${2:?"missing CMake minor version"}"
  declare -r patch="${3:?"missing CMake patch version"}"
  declare -r version="${major}.${minor}.${patch}"

  declare -r cache_dir="$HOME/.cache/mongo-c-driver"
  declare -r cmake_cache_dir="${cache_dir}/cmake-${version}"
  declare -r cmake_binary="${cmake_cache_dir}/bin/cmake"

  if command -v "${cmake_binary}" >/dev/null; then
    if "${cmake_binary}" --version | grep -q "${version}"; then
      echo "Found CMake ${version} in mongo-c-driver CMake cache" 1>&2
      printf "%s" "${cmake_binary}"
      return # No work to be done: required CMake binary already exists in cache.
    fi

    # Shouldn't happen, but log if it does.
    echo "Found inconsistent CMake version in mongo-c-driver CMake cache" 1>&2
  fi

  declare platform
  declare extension
  declare decompressor
  declare decompressor_args
  declare root_dir

  # This logic is only applicable to CMake 3.20 and newer. For CMake versions
  # earlier than 3.20 or CMake versions without prebuild binaries for certain
  # platforms fallback to building from source.
  case "${OSTYPE:?}-${HOSTTYPE:?}" in
  cygwin-*)
    platform="windows-${HOSTTYPE}"
    extension="zip"
    decompressor="unzip"
    decompressor_args=("-q")
    root_dir="cmake-${version}-${platform}"
    ;;
  darwin*-*)
    platform="macos-universal"
    extension="tar.gz"
    decompressor="tar"
    decompressor_args=("xzf")
    root_dir="cmake-${version}-${platform}/CMake.app/Contents"
    ;;
  linux*-s390x | linux*-powerpc64le) ;; # Build from source.
  linux*-*)
    platform="linux-${HOSTTYPE}"
    extension="tar.gz"
    decompressor="tar"
    decompressor_args=("xzf")
    root_dir="cmake-${version}-${platform}"
    ;;
  *) ;; # Build from source.
  esac

  declare -a curl_args
  curl_args=(
    "--retry" "5"
    "-sS"
    "--max-time" "120"
    "--fail"
  )

  # TODO: remove once BUILD-16817 is resolved.
  case "${distro_id:?}" in
  rhel71-power8-* | ubuntu1404-* | ubuntu1604-power8-*)
    # Workaround SSL certificate verification failures on certain distros.
    curl_args+=("-k")
    ;;
  esac

  {
    # Doesn't matter who creates the cache directory so long as it exists.
    mkdir -p "${cache_dir}" || return

    # Avoid polluting current working directory.
    declare tmp_cmake_dir
    tmp_cmake_dir="$(make_tmpdir_in "${cache_dir}")" || return
  } 1>&2

  if [[ -n "${platform}" ]]; then
    cmake_download_binary() (
      declare -r cmake_url="https://cmake.org/files/v${major}.${minor}/cmake-${version}-${platform}.${extension}"

      echo "Downloading cmake-${version}-${platform}..."

      cd "${tmp_cmake_dir}" || return

      # Allow download to fail and fallback to building from source.
      if curl "${curl_args[@]}" "${cmake_url}" --output "cmake.${extension}"; then
        "${decompressor}" "${decompressor_args[@]}" "cmake.${extension}" || return

        cmake_replace_latest "${cache_dir}" "$(pwd)/${root_dir}" "${version}" || return

        # Verify CMake binary works as expected.
        command -v "${cmake_binary}" || return

        echo "Downloading cmake-${version}-${platform}... done."
      else
        echo "Could not find prebuild binaries for cmake-${version}-${platform}."
      fi
    ) 1>&2

    cmake_download_binary || return

    if command -v "${cmake_binary}" >/dev/null && "${cmake_binary}" --version | grep -q "${version}"; then
      # Successfully downloaded binaries. No more work to be done.
      printf "%s" "${cmake_binary}"
      return
    fi
  fi

  # Could not obtain a prebuild binary; build CMake from source instead.
  (
    declare -r cmake_url="https://cmake.org/files/v${major}.${minor}/cmake-${version}.tar.gz"

    echo "Building cmake-${version} from source..."

    cd "${tmp_cmake_dir}" || return

    curl "${curl_args[@]}" "${cmake_url}" --output "cmake.tar.gz" || return
    tar xzf cmake.tar.gz || return

    if [[ "${OSTYPE}" == darwin* ]]; then
      nproc() { sysctl -n hw.logicalcpu; } # MacOS does not have nproc.
    fi

    declare -a bootstrap_args
    bootstrap_args=(
      "--prefix=$(pwd)/install-dir"
      "--parallel=$(nproc)"
    ) || return

    # Make an attempt to improve CMake build speed by opting into ccache.
    export PATH
    if command -v /opt/mongodbtoolchain/v4/bin/ccache; then
      PATH="${PATH}:/opt/mongodbtoolchain/v4/bin"
      bootstrap_args+=("--enable-ccache")
    elif command -v /opt/mongodbtoolchain/v3/bin/ccache; then
      PATH="${PATH}:/opt/mongodbtoolchain/v3/bin"
      bootstrap_args+=("--enable-ccache")
    fi

    pushd "cmake-${version}" || return
    {
      ./bootstrap "${bootstrap_args[@]}" || return
      make -j "$(nproc)" install || return
    } >/dev/null   # Only interested in errors.
    popd || return # "cmake-${version}"

    cmake_replace_latest "${cache_dir}" "$(pwd)/install-dir" "${version}" || return

    # Verify CMake binary works as expected.
    command -v "${cmake_binary}" || return

    echo "Building cmake-${version} from source... done."
  ) 1>&2

  printf "%s" "${cmake_binary}"
}
