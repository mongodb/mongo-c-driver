# Run with a . to add environment variables to the current shell:
# . ./set-temp-creds.sh
#
# After running the following variables are set:
# - CSFLE_AWS_TEMP_ACCESS_KEY_ID
# - CSFLE_AWS_TEMP_SECRET_ACCESS_KEY
# - CSFLE_AWS_TEMP_SESSION_TOKEN
#

set +o xtrace # Disable tracing.
# set -o errexit

CREDS=$(python print-temp-creds.py)
PYTHON=${PYTHON:-python}

export CSFLE_AWS_TEMP_ACCESS_KEY_ID=$(echo $CREDS | awk '{print $1}')
export CSFLE_AWS_TEMP_SECRET_ACCESS_KEY=$(echo $CREDS | awk '{print $2}')
export CSFLE_AWS_TEMP_SESSION_TOKEN=$(echo $CREDS | awk '{print $3}')