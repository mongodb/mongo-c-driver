#!/bin/sh

# CDRIVER-2751: remove this workaround after awscli always comes pre-installed.
set -o xtrace
set -o errexit

if ! which aws; then
    echo "awscli not found, installing in python virtualenv"
    if [ -z $VIRTUAL_ENV ]; then
      python -m virtualenv venv
      cd venv
      . bin/activate
    fi
    ./bin/pip install awscli
    cd ..
fi

aws --version