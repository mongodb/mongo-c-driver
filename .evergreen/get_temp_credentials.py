"""
Obtains temporary AWS credentials for CSFLE testing.

This script needs to use the AWS credentials for CSFLE to obtain temporary credentials.
Those credentials can be passed through the following environment variables:

AWS_ACCESS_KEY_ID=...
AWS_SECRET_ACCESS_KEY=...
AWS_DEFAULT_REGION=us-east-1
"""

import boto3

client = boto3.client('sts')
credentials = client.get_session_token()["Credentials"]
print (credentials["AccessKeyId"] + " " + credentials["SecretAccessKey"] + " " + credentials["SessionToken"])