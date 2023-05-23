#!/usr/bin/env bash

## Variables set by this file:
#
# * OS_NAME
#   • One of ‘windows’, ‘linux’, ‘macos’, or ‘unknown’

. "$(dirname "${BASH_SOURCE[0]}")/use.sh" base

# Get the platform name: One of 'windows', 'macos', 'linux', or 'unknown'
have-command uname || fail "No 'uname' executable found"

debug "Uname is [$(uname -a)]"
declare _uname
_uname="$(uname -a | tr '[:upper:]' '[:lower:]')"
declare _os_name="unknown"

if [[ "$_uname" =~ .*cygwin|windows|mingw|msys.* ]] || (have-command cmd.exe && ! [[ $_uname =~ .*wsl.* ]]); then
    # We are running on Windows, and not within a WSL environment
    _os_name="windows"
elif [[ $_uname =~ darwin.* ]]; then
    _os_name='macos'
elif [[ $_uname =~ linux.* ]]; then
    _os_name='linux'
fi

declare -r OS_NAME=$_os_name
debug "Detected OS_NAME is [$OS_NAME]"
