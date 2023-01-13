#!/usr/bin/env bash

# bypass_dlclose
#
# Usage:
#   bypass_dlclose command args...
#
# Parameters:
#   "$@": command and arguments to evaluate.
#   "$CC": compiler to use to compile and link the bypass_dlclose library.
#
# Evaluates the provided command and arguments with LD_PRELOAD defined to
# preload a `bypass_dlclose.so` library defining a no-op `dlclose()`.
# If necessary, also preloads the `libasan.so` library to satisfy linker
# requirements.
bypass_dlclose() {
  : "${1:?'bypass_dlclose expects at least one argument to run as command!'}"
  : "${CC:?'bypass_dlclose expects environment variable CC to be defined!'}"

  declare tmp

  if ! tmp="$(mktemp -d)"; then
    echo "Could not create temporary directory for bypass_dlclose library!" 1>&2
    return 1
  fi
  trap 'rm -rf "$tmp"' EXIT

  declare ld_preload

  echo "int dlclose (void *handle) {(void) handle; return 0; }" >|"$tmp/bypass_dlclose.c"

  "$CC" -o "$tmp/bypass_dlclose.so" -shared "$tmp/bypass_dlclose.c" || return

  ld_preload="$tmp/bypass_dlclose.so"

  # Clang uses its own libasan.so; do not preload it!
  if [ "$CC" != "clang" ]; then
    declare asan_path
    asan_path="$($CC -print-file-name=libasan.so)" || return
    ld_preload="$asan_path:$ld_preload"
  fi

  LD_PRELOAD="$ld_preload:${LD_PRELOAD:-}" "$@"
}
