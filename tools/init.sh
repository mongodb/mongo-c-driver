#!/bin/bash

# Initial variables and helper functions for build and tooling scripts

## Variables set by this file:

# TOOLS_DIR
#   • The path to the directory containing this script file
# MONGOC_DIR
#   • The path to the top-level project directory
# USER_CACHES_DIR
#   • A user-local directory in which to store cached data (can be overriden)
# OS_NAME
#   • One of ‘windows’, ‘linux’, ‘macos’, or ‘unknown’
# EXE_SUFFIX
#   • Expands to “.exe” on Windows, otherwise an empty string

## (All of the above directory paths are native absolute paths)

## This script defines the following commands:

# * to_absolute <path>
#       Convert a given path into an absolute path. Relative paths are
#       resolved relative to the working directory.
#
# * native_path <path>
#       On MinGW/Cygwin/MSYS, convert the given Cygwin path to a Windows-native
#       path. On other platforms, the argument is unmodified
#
# * have_command <command>
#       Return zero if <command> is the name of a command that can be executed,
#       returns non-zero otherwise.
#
# * run_chdir <dirpath> <command> [args ...]
#       Run the given command with a working directory given by <dirpath>
#
# * find_python
#       Get the path to a Python 3 executable
#
# * run_python [command...]
#       Execute the newest working Python 3 executable from $(find_python)
#
# * log <message>
#       Print <message> to stderr
#
# * fail <message>
#       Print <message> to stderr and return non-zero
#
# * run_with_lock <lockfile> [command ...]
#       Execute ‘command’ while holding <lockfile> as an exclusive lock. This
#       requires the ‘lckdo’ command, otherwise it executes the command without
#       taking any lock. The parent directory of <lockfile> must exists. NOTE:
#       the given command must be an application, and not a shell-internal or
#       shell function.
#

set -o errexit
set -o pipefail
set -o nounset

# Inhibit msys path conversion
export MSYS2_ARG_CONV_EXCL="*"

# Write a message to stderr
function log() {
    echo "${@}" 1>&2
    return 0
}

# Write a message to stderr if $DEBUG is not empty/zero
function debug() {
    if [[ "${DEBUG:-0}" != "0" ]]; then
        log "${@}"
    fi
}

# Print a message and return non-zero
function fail() {
    log "${@}"
    return 1
}

# Determine whether the given string names an executable command
function have_command() {
    [[ "$#" -eq 1 ]] || fail "have_command expects a single argument"
    if type "${1}" > /dev/null 2>&1; then
        return 0
    fi
    return 1
}

# Run a command in a different directory:
# * run_chdir <dir> [command ...]
function run_chdir() {
    [[ "$#" -gt 1 ]] || fail "run_chdir expects at least two arguments"
    local _dir="$1"
    shift
    pushd "$_dir" > /dev/null
    debug "Run in directory [$_dir]:" "$@"
    "$@"
    local _rc=$?
    popd > /dev/null
    return $_rc
}

# "1" if we have a cygpath.exe available, otherwise "0"
_HAVE_CYGPATH=0
if have_command cygpath; then
    _HAVE_CYGPATH=1
fi

# Given a path string, convert it to an absolute path with no redundant components or directory separators
function to_absolute() {
    [[ "$#" -eq 1 ]] || fail "to_absolute expects a single argument"
    local ret
    local arg="$1"
    debug "Resolve path [$arg]"

    # Cygpath can resolve the path in a single subprocess:
    if [[ $_HAVE_CYGPATH = 1 ]]; then
        # Ask Cygpath to resolve the path. It knows how to do it reliably and quickly:
        ret=$(cygpath --absolute --mixed --long-name -- "$arg")
        debug "Cygpath resolved: [$arg]"
        printf %s "$ret"
        return 0
    fi

    # If the given directory exists, we can ask the shell to resolve the path
    # by going there and asking the PWD:
    if [[ -d $arg ]]; then
        ret=$(run_chdir "$arg" pwd)
        debug "Resolved: [$arg]"
        printf %s "$ret"
        return 0
    fi

    # Do it the "slow" way:

    # The parent path:
    local _parent
    _parent="$(dirname "$arg")"
    # The filename part:
    local _fname
    _fname="$(basename "$arg")"
    # There are four cases to consider from dirname:
    if [[ $_parent = "." ]]; then  # The parent is '.' as in './foo'
        # Replace the leading '.' with the working directory
        _parent="$(pwd)"
    elif [[ $_parent = ".." ]]; then  # The parent is '..' as in '../foo'
        # Replace a leading '..' with the parent of the working directory
        _parent="$(dirname "$(pwd)")"
    elif [[ $arg == "$_parent" ]]; then  # The parent is itself, as in '/'
        # A root directory is its own parent according to 'dirname'
        printf %s "$arg"
        return 0
    else  # The parent is some other path, like 'foo' in 'foo/bar'
        # Resolve the parent path
        _parent="$(set +x; DEBUG=0 to_absolute "$_parent")"
    fi
    # At this point $_parent is an absolute path
    if [[ $_fname = ".." ]]; then
        # Strip one component
        ret="$(dirname "$_parent")"
    elif [[ $_fname = "." ]]; then
        # Drop a '.' at the end of a path
        ret="$_parent"
    else
        # Join the result
        ret="$_parent/$_fname"
    fi
    # Remove duplicate dir separators
    while [[ $ret =~ "//" ]]; do
        ret="${ret//\/\///}"
    done
    debug "Resolved path: [$arg] → [$ret]"
    printf %s "$ret"
}

# Get the platform name: One of 'windows', 'macos', 'linux', or 'unknown'
__os_name() {
    have_command uname || fail "No 'uname' executable found"

    debug "Uname is [$(uname -a)]"
    local _uname
    _uname="$(uname -a | tr '[:upper:]' '[:lower:]')"
    local ret="unknown"

    if [[ "$_uname" =~ .*cygwin|windows|mingw|msys.* ]] || (have_command cmd.exe && ! [[ $_uname =~ .*wsl.* ]]); then
        # We are running on Windows, and not within a WSL environment
        ret="windows"
    elif [[ $_uname =~ darwin.* ]]; then
        ret='macos'
    elif [[ $_uname =~ linux.* ]]; then
        ret='linux'
    fi

    printf %s "$ret"
}

OS_NAME="$(__os_name)"

# Ensure the given path is a native path (converts Cygwin paths to Windows-local paths)
function native_path() {
    [[ "$#" -eq 1 ]] || fail "native_path expects exactly one argument"
    declare arg=$1
    if [[ "$OS_NAME" = "windows" ]]; then
        have_command cygpath || fail "No 'cygpath' command is available, but we require it to normalize file paths on Windows."
        local r
        r="$(cygpath -w "$arg")"
        debug "Convert path [$arg] → [$r]"
        printf %s "$r"
    else
        printf %s "$1"
    fi
}

# Run a command with a lock held:
# * run_with_lock <lockfile> [<command> ...]
function run_with_lock() {
    [[ "$#" -gt 1 ]] || fail "run_with_lock requires a lock filename and a command to run"
    if ! have_command lckdo; then
        log "No ‘lckdo’ program is installed. We'll run without the lock, but parallel tasks may contend."
        log "  (‘lckdo’ is part of the ‘moreutils’ package)"
        shift
        command "$@"
    else
        lckdo -W 30 -- "$@"
    fi
}


_init_sh_this_file="$(to_absolute "${BASH_SOURCE[0]}")"
_init_sh_tools_dir="$(dirname "${_init_sh_this_file}")"

# Get the tools dir as a native absolute path. All other path vars are derived from
# this one, and will therefore remain as native paths
TOOLS_DIR="$(native_path "${_init_sh_tools_dir}")"
MONGOC_DIR="$(dirname "${TOOLS_DIR}")"

find_python() {
    pys=(
        py
        python3.14
        python3.13
        python3.12
        python3.11
        python3.10
        python3.9
        python3.8
        python3
        python
    )
    for cand in "${pys[@]}"; do
        if have_command "$cand" && "$cand" -c "(x:=0)" > /dev/null 2>&1; then
            printf %s "$cand"
            return
        fi
    done
    fail "No Python (≥3.8) executable was found"
}

run_python() {
    declare py
    py=$(find_python)
    "$py" "$@"
}

EXE_SUFFIX=""
if [[ "$OS_NAME" = "windows" ]]; then
    EXE_SUFFIX=".exe"
fi

if [[ "${USER_CACHES_DIR:=${XDG_CACHE_HOME:-}}" = "" ]]; then
    case "$OS_NAME" in
    linux)
        USER_CACHES_DIR=$HOME/.cache
        ;;
    macos)
        USER_CACHES_DIR=$HOME/Library/Caches
        ;;
    windows)
        USER_CACHES_DIR=${LOCALAPPDATA:-$USERPROFILE/.cache}
        ;;
    *)
        log "Using ~/.cache as fallback user caching directory"
        USER_CACHES_DIR="$(to_absolute ~/.cache)"
    esac
fi

# Ensure we are dealing with a complete path
USER_CACHES_DIR="$(to_absolute "$USER_CACHES_DIR")"

: "${BUILD_CACHE_BUST:=1}"
: "${BUILD_CACHE_DIR:="$USER_CACHES_DIR/mongoc/build.$BUILD_CACHE_BUST"}"

# Silence shellcheck:
: "$MONGOC_DIR,$EXE_SUFFIX"
