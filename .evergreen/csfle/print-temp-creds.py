import boto3

client = boto3.client('sts')
credentials = client.get_session_token()["Credentials"]
print (credentials["AccessKeyId"] + " " + credentials["SecretAccessKey"] + " " + credentials["SessionToken"])