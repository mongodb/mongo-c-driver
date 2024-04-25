#!/bin/bash

# Because drivers-evergreen-tools refers to python3 as python, we must be sure
# that we both have a python3 interpreter installed and also ensure that
# invoking 'python' will use a Python 3 interpreter.

if [[ "$(python3 --version)" =~ "Python 3" ]]; then
    echo "Python 3 is installed"
    python3 --version
fi
    echo "Python 3 is NOT installed"
    exit
else

# Setup virtualenv such that invoking python invokes python3
virtualenv venv -p `which python3`
. venv/bin/activate

# Setup OIDC token in /tmp/tokens/
export AWS_PROFILE="drivers-test-secrets-role-857654397073"
$DIR/../../../drivers-evergreen-tools/.evergreen/auth_oidc/oidc_get_tokens.sh
