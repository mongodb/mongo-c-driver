# Obtains temporary AWS credentials for CSFLE testing.
#
# Run with a . to add environment variables to the current shell:
# . ./set-temp-creds.sh
#
# Requires the python AWS SDK boto3. This can be installed with: pip install boto3
# The path to python in a virtual environment may be passed with the PYTHON
# environment variable.
#
# Requires AWS credentials for CSFLE to obtain temporary credentials.
# Those credentials can be passed through the following environment variables:
#
# export AWS_ACCESS_KEY_ID=...
# export AWS_SECRET_ACCESS_KEY=...
# export AWS_DEFAULT_REGION=us-east-1
#
# After running this script, the following shell variables are set:
# - CSFLE_AWS_TEMP_ACCESS_KEY_ID
# - CSFLE_AWS_TEMP_SECRET_ACCESS_KEY
# - CSFLE_AWS_TEMP_SESSION_TOKEN
#

set +o xtrace # Disable tracing.

get_creds() {
    $PYTHON - "$@" << 'EOF'
import boto3

client = boto3.client("sts")
credentials = client.get_session_token()["Credentials"]
print (credentials["AccessKeyId"] + " " + credentials["SecretAccessKey"] + " " + credentials["SessionToken"])
EOF
}

PYTHON=${PYTHON:-python}
CREDS=$(get_creds)

export CSFLE_AWS_TEMP_ACCESS_KEY_ID=$(echo $CREDS | awk '{print $1}')
export CSFLE_AWS_TEMP_SECRET_ACCESS_KEY=$(echo $CREDS | awk '{print $2}')
export CSFLE_AWS_TEMP_SESSION_TOKEN=$(echo $CREDS | awk '{print $3}')