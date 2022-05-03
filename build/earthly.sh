#!/usr/bin/bash

set -euo pipefail

: "${EARTHLY_VERSION:=0.6.14}"

# Filename executable suffix
suffix=""

# Calc the arch of the executable we want
arch="$(uname -m)"
case "$arch" in
    x86_64)
        arch=amd64
        ;;
    aarch64|arm64)
        arch=arm64
        ;;
    *)
        echo "Unknown architecture: $arch" 1>&1
        exit 99
        ;;
esac

# Calc the name of the platform and the caching directory
os="$(uname -s)"
case "$os" in
    Linux)
        os=linux
        caches_root="${XDG_CACHES_HOME:-$HOME/.cache}"
        ;;
    Darwin)
        os=darwin
        caches_root="${XDG_CACHES_HOME:-$HOME/Library/Caches}"
        ;;
    Windows*|MINGW*|CYGWIN*)
        os=windows
        suffix=.exe
        caches_root="$LocalAppData"
        ;;
    *)
        echo "Unknown operating system: $os" 1>&2
        exit 98
        ;;
esac

cache_dir="$caches_root/earthly-sh/$EARTHLY_VERSION"
mkdir -p "$cache_dir"

exe_filename="earthly-$os-$arch$suffix"
exe_path="$cache_dir/$exe_filename"

if ! test -f "$exe_path"; then
    echo "Downloading $exe_filename $EARTHLY_VERSION"
    url="https://github.com/earthly/earthly/releases/download/v$EARTHLY_VERSION/$exe_filename"
    curl -Ls "$url" -o "$exe_path"
fi

chmod a+x "$exe_path"

"$exe_path" "$@"
