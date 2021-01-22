import boto3

"""
Obtains temporary AWS credentials for CSFLE testing.

Requires the AWS SDK boto3. This can be installed with: pip install boto3

This script needs to use the AWS credentials for CSFLE to obtain temporary credentials.
Those credentials can be passed through the following environment variables:

AWS_ACCESS_KEY_ID=...
AWS_SECRET_ACCESS_KEY=...
AWS_DEFAULT_REGION=us-east-1

Use the set-temp-creds.sh script to set the resulting temporary credentials as output
environment variables.
"""

client = boto3.client('sts')
credentials = client.get_session_token()["Credentials"]
print (credentials["AccessKeyId"] + " " + credentials["SecretAccessKey"] + " " + credentials["SessionToken"])