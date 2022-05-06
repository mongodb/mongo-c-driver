#!/usr/bin/env bash

set -eo pipefail

if test -n "${CLIENT_SIDE_ENCRYPTION:-}"; then
  test -f secrets.json
  set +x
  export MONGOC_TEST_AWS_SECRET_ACCESS_KEY="$(jq -r .aws.secretAccessKey secrets.json)"
  export MONGOC_TEST_AWS_ACCESS_KEY_ID="$(jq -r .aws.accessKeyId secrets.json)"
  export MONGOC_TEST_AZURE_TENANT_ID="$(jq -r .azure.tenantId secrets.json)"
  export MONGOC_TEST_AZURE_CLIENT_ID="$(jq -r .azure.clientId secrets.json)"
  export MONGOC_TEST_AZURE_CLIENT_SECRET="$(jq -r .azure.clientSecret secrets.json)"
  export MONGOC_TEST_GCP_EMAIL="$(jq -r .gcp.email secrets.json)"
  export MONGOC_TEST_GCP_PRIVATEKEY="$(jq -r .gcp.privateKey secrets.json)"
  export MONGOC_TEST_CSFLE_TLS_CA_FILE=../drivers-evergreen-tools/.evergreen/x509gen/ca.pem
  export MONGOC_TEST_CSFLE_TLS_CERTIFICATE_KEY_FILE=../drivers-evergreen-tools/.evergreen/x509gen/client.pem
fi

bash .evergreen/integration-tests.sh

echo "Starting mock KMS servers..."
pushd ./drivers-evergreen-tools/.evergreen/csfle
  . ./activate_venv.sh
  python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --port 7999 &
  python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/expired.pem --port 8000 &
  python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/wrong-host.pem --port 8001 &
  python -u kms_http_server.py --ca_file ../x509gen/ca.pem --cert_file ../x509gen/server.pem --port 8002 --require_client_cert &
  python -u kms_kmip_server.py &
  echo "Starting mock KMS servers... done."
popd

sh .evergreen/run-tests.sh
