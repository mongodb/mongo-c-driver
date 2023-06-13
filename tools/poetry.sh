#!/usr/bin/env bash

# Automatically installs and invokes a pinned version of Poetry (https://python-poetry.org/)
#
## Commands defined by this file:
#
# * run-poetry [<cmd> ...]
#     • Execute the given Poetry command. This script can also be executed
#       directly to run the same Poetry command.
#
#       On first run, will install a new Poetry instance in a user-local cache
#       directory. This script DOES NOT respect POETRY_HOME! Instead, use
#       FORCE_POETRY_HOME to force a directory in which to install Poetry.
#
# * ensure-poetry [<version> [<home>]]
#     • Ensures that Poetry of the given <version> is installed into <home>.
#       This is an idempotent operation. Defaults are from WANT_POETRY_VERSION
#       and POETRY_HOME (see below).
#
## Variables set by this file:
#
# * POETRY_HOME
#     • The default user-local directory in which Poetry will be installed.
#       This can be overriden by setting FORCE_POETRY_HOME.
# * POETRY_EXE
#     • The default full path to the Poetry that will be installed and run by
#       this script (not present until after ensure-poetry or run-poetry is
#       executed).
# * WANT_POETRY_VERSION (overridable) (default 1.4.2)
#     • The version of Poetry that will be installed by run-poetry when executed.

# Load vars and utils:
. "$(dirname "${BASH_SOURCE[0]}")/use.sh" python paths base with_lock platform

: "${WANT_POETRY_VERSION:=1.4.2}"
declare -r -x POETRY_HOME=${FORCE_POETRY_HOME:-"$BUILD_CACHE_DIR/poetry-$WANT_POETRY_VERSION"}
declare -r POETRY_EXE=$POETRY_HOME/bin/poetry$EXE_SUFFIX

_POETRY_PIP_INSTALL_ARGS=(
    --disable-pip-version-check
    --quiet
    poetry=="$WANT_POETRY_VERSION"
)

if $IS_POWERPC || $IS_ZSERIES; then
    # Needed for PowerPC and s390x, which doesn't have binary Cryptography wheels for our version,
    # and our CI does not have the Rust toolchain available to build one:
    _POETRY_PIP_INSTALL_ARGS+=("cryptography<3.3")
fi

install-poetry() {
    log "Creating virtualenv for Poetry…"
    run-python -m venv "$POETRY_HOME"
    log "Updating pip…"
    "$POETRY_HOME/bin/python$EXE_SUFFIX" -m pip install --quiet -U pip
    log "Installing Poetry…"
    "$POETRY_HOME/bin/python$EXE_SUFFIX" -m pip install "${_POETRY_PIP_INSTALL_ARGS[@]}"
    is-file "$POETRY_EXE" || fail "Installation did not create the expected executable [$POETRY_EXE]"
    log "Installing Poetry… - Done"
    printf %s "$WANT_POETRY_VERSION" > "$POETRY_HOME/installed.txt"
}

# Idempotent installation:
# Usage: ensure-poetry <version> <poetry-home>
ensure-poetry() {
    declare version=${1:-$WANT_POETRY_VERSION}
    declare home=${2:-$POETRY_HOME}
    if is-file "$home/installed.txt" && [[ "$(cat "$home/installed.txt")" == "$version" ]]; then
        return 0
    fi
    mkdir -p "$POETRY_HOME"
    with-lock "$POETRY_HOME/.install.lock" \
        env WANT_POETRY_VERSION="$WANT_POETRY_VERSION" \
            FORCE_POETRY_HOME="$POETRY_HOME" \
        "$BASH" "$TOOLS_DIR/poetry.sh" --install
    is-file "$POETRY_EXE" || fail "Installation did not create the expected executable [$POETRY_EXE]"
}

run-poetry() {
    ensure-poetry "$WANT_POETRY_VERSION" "$POETRY_HOME"
    env POETRY_HOME="$POETRY_HOME" "$POETRY_EXE" "$@"
}

# Poetry bug: Poetry uses the keyring, even for non-authenticating commands,
# which can wreak havoc in cases where the keyring is unavailable
# (i.e. SSH and non-interactive sessions)
# (https://github.com/python-poetry/poetry/issues/1917)
export PYTHON_KEYRING_BACKEND="keyring.backends.null.Keyring"

if is-main; then
    if [[ "$*" = "--ensure-installed" ]]; then
        # Just install, don't run it
        ensure-poetry "$WANT_POETRY_VERSION" "$POETRY_HOME"
    elif [[ "${1:-}" = "--install" ]]; then
        shift
        install-poetry "$@"
    elif [[ "$*" = "--env-use" ]]; then
        _python=$(find-python)
        run-poetry env use -- "$_python"
    else
        # Run the Poetry command:
        run-poetry "$@"
    fi
fi
