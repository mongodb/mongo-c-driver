#!/usr/bin/env bash

: <<EOF

Source this script to import components into other scripts. Usage:

    source <path-to-use.sh> <name> [<name> [...]]

This script will enable strict mode in the parent. <name> refers to the filepath
stems of scripts that are sibling to use.sh (i.e. within the same directory, the
name of "foo.sh" is "foo").

Commands defined by importing this file:

  is-main
    • This command takes no arguments, and returns zero if the invoked in a
      while imports are not being resolved (i.e. the script is being executed
      directly rather than being imported)

EOF

set -o errexit
set -o pipefail
set -o nounset

# Grab the absolute path to the directory of this script:
pushd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null
_this_dir=$(pwd)
popd >/dev/null

# We use base components, so implicitly import that:
. "$_this_dir/base.sh"
_IMPORTED_base=1

# A utility that exits true if invoked outside of an importing context:
is-main() { [[ $_IS_IMPORTING = 0 ]]; }

# Keep a stack, of scripts being imported,:
declare -a _USE_IMPORTING

# Inform scripts that they are being imported, not executed directly:
_IS_IMPORTING=1

for item in "$@"; do
    # Don't double-import items:
    if is-set "_IMPORTED_$item"; then
        continue
    fi
    # Push this item:
    _USE_IMPORTING+=("$item")
    # The file to be imported:
    file=$_this_dir/$item.sh
    debug "Import: [$item]"
    _err=0
    # Detect self-import:
    if printf '%s\0' "${BASH_SOURCE[@]}" | grep -qFxZ -- "$file"; then
        log "File '$file' imports itself transitively"
        _err=1
    fi
    # Detect non-existing imports:
    if ! is-file "$file"; then
        log "No script '$file' exists to import."
        _err=1
    fi
    # Print the stacktrace of imports upon error:
    if [[ $_err -eq 1 ]]; then
        printf " • [%s] loaded by:\n" "${BASH_SOURCE[@]}" 1>&2
        log " • (user)"
        fail "Bailing out"
    fi
    # shellcheck disable=1090
    . "$file"
    # Recover item from the stack, since we may have recursed:
    item="${_USE_IMPORTING[${#_USE_IMPORTING[@]}-1]}"
    # Pop the top stack item:
    unset "_USE_IMPORTING[${#_USE_IMPORTING[@]}-1]"
    # Declare that the item has been imported, for future reference:
    declare "_IMPORTED_$item=1"
    debug "Import: [$item] - done"
done

# Set _IS_IMPORTING to zero if the import stack is empty
if [[ "${_USE_IMPORTING+${_USE_IMPORTING[*]}}" = "" ]]; then
    _IS_IMPORTING=0
fi
