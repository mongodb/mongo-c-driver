#!/usr/bin/env bash

## Variables set by this file:
#
# * IS_DARWIN, IS_WINDOWS, IS_LINUX, IS_BSD, IS_WSL, IS_UNIX_LIKE
#   • Set to either "true" or "false" depending on the host operating system.
#     More than one value may be true (i.e. $IS_WSL and $IS_WINDOWS). Because
#     "true" and "false" are Bash built-ins, these can be used in conditionals
#     directly, as in "if $IS_WINDOWS || $IS_DARWIN; then …"
# * OS_FAMILY
#   • One of "windows", "linux", "darwin", or "bsd", depending on the host
#     operating system.

. "$(dirname "${BASH_SOURCE[0]}")/use.sh" base

_is_darwin=false
_is_windows=false
_is_linux=false
_is_unix_like=false
_is_wsl=false
_is_bsd=false
_os_family=unknown
case "$OSTYPE" in
    linux-*)
        if have-command cmd.exe; then
            _is_wsl=true
            _is_unix_like=true
            _os_family=windows
        else
            _is_linux=true
            _is_unix_like=true
            _os_family=linux
        fi
        ;;
    darwin*)
        _is_darwin=true
        _is_unix_like=true
        _os_family=darwin
        ;;
    FreeBSD|openbsd*|netbsd)
        _is_bsd=true
        _is_unix_like=true
        _os_family=bsd
        ;;
    msys*|cygwin*)
        _is_windows=true
        _os_family=windows
        ;;
esac

declare -r IS_DARWIN=$_is_darwin
declare -r IS_WINDOWS=$_is_windows
declare -r IS_LINUX=$_is_linux
declare -r IS_UNIX_LIKE=$_is_unix_like
declare -r IS_WSL=$_is_wsl
declare -r IS_BSD=$_is_bsd
declare -r OS_FAMILY=$_os_family

if is-main; then
    log "Operating system detection:"
    log "  • OS_FAMILY: $OS_FAMILY"
    log "  • IS_WINDOWS: $IS_WINDOWS"
    log "  • IS_DARWIN: $IS_DARWIN"
    log "  • IS_LINUX: $IS_LINUX"
    log "  • IS_BSD: $IS_BSD"
    log "  • IS_WSL: $IS_WSL"
    log "  • IS_UNIX_LIKE: $IS_UNIX_LIKE"
fi
