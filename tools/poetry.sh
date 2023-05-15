#!/usr/bin/env bash

# Automatically installs and invokes a pinned version of Poetry (https://python-poetry.org/)
# Usage:
#
# * bash poetry.sh [poetry-args...]
#
# On first run, will install a new Poetry instance in a user-local cache directory.
# This script DOES NOT respect POETRY_HOME! Instead, use FORCE_POETRY_HOME to
# force a directory in which to install Poetry.
#
# The following variables can be uesd to control this script:
#
#   • WANT_POETRY_VERSION → What version of Poetry to try and install. (Default is 1.4.2)
#   • FORCE_POETRY_HOME → Override the default generated POETRY_HOME.

# Load vars and utils:
. "$(dirname "${BASH_SOURCE[0]}")/init.sh"

# The Poetry install script, which we keep locally
install_py="$TOOLS_DIR/install-poetry.py"

: "${WANT_POETRY_VERSION:=1.4.2}"
: "${POETRY_HOME=${FORCE_POETRY_HOME:-"$BUILD_CACHE_DIR/poetry-$WANT_POETRY_VERSION"}}"
export POETRY_HOME

# Idempotent installation:
if [[ ! -f "$POETRY_HOME/.installed.done" ]]; then
    log "Installing Poetry $WANT_POETRY_VERSION into [$POETRY_HOME]"
    py=$(find_python)
    mkdir -p "$POETRY_HOME"
    run_with_lock "$POETRY_HOME/.install.lock" \
        "$py" -u "$install_py" --yes --version "$WANT_POETRY_VERSION" \
    || (
        cat -- poetry-installer*.log && fail "Poetry installation failed"
    )
    touch "$POETRY_HOME/.installed.done"
fi

# Poetry bug: Poetry uses the keyring, even for non-authenticating commands,
# which can wreak havoc in cases where the keyring is unavailable
# (i.e. SSH and non-interactive sessions)
# (https://github.com/python-poetry/poetry/issues/1917)
export PYTHON_KEYRING_BACKEND="keyring.backends.null.Keyring"

# Run the Poetry command:
"$POETRY_HOME/bin/poetry" "$@"
