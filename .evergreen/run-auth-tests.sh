#!/usr/bin/env bash

set -o errexit
set -o pipefail

set +o xtrace # Don't echo commands

to_absolute() (
  cd "${1:?}" && pwd
)

declare script_dir
script_dir="$(to_absolute "$(dirname "${BASH_SOURCE[0]}")")"

declare mongoc_dir
mongoc_dir="$(to_absolute "${script_dir}/..")"

declare install_dir="${mongoc_dir}/install-dir"

declare c_timeout="connectTimeoutMS=30000&serverSelectionTryOnce=false"

declare sasl="OFF"
if grep -q "#define MONGOC_ENABLE_SASL 1" src/libmongoc/src/mongoc/mongoc-config.h; then
  sasl="ON"
fi

declare ssl="OFF"
if grep -q "#define MONGOC_ENABLE_SSL 1" src/libmongoc/src/mongoc/mongoc-config.h; then
  ssl="ON"
fi

# shellcheck source=.evergreen/add-build-dirs-to-paths.sh
. "${script_dir}/add-build-dirs-to-paths.sh"

# shellcheck source=.evergreen/bypass-dlclose.sh
. "${script_dir}/bypass-dlclose.sh"

declare ping
declare test_gssapi
declare ip_addr
case "${OSTYPE}" in
cygwin)
  ping="./src/libmongoc/Debug/mongoc-ping.exe"
  test_gssapi="./src/libmongoc/Debug/test-mongoc-gssapi.exe"
  ip_addr="$(getent hosts "${auth_host:?}" | head -n 1 | awk '{print $1}')"
  ;;

darwin*)
  ping="./src/libmongoc/mongoc-ping"
  test_gssapi="./src/libmongoc/test-mongoc-gssapi"
  ip_addr="$(dig "${auth_host:?}" +short | tail -1)"
  ;;

*)
  ping="./src/libmongoc/mongoc-ping"
  test_gssapi="./src/libmongoc/test-mongoc-gssapi"
  ip_addr="$(getent hosts "${auth_host:?}" | head -n 1 | awk '{print $1}')"
  ;;
esac
: "${ping:?}"
: "${test_gssapi:?}"
: "${ip_addr:?}"

if command -v kinit && [[ -f /tmp/drivers.keytab ]]; then
  kinit -k -t /tmp/drivers.keytab -p drivers@LDAPTEST.10GEN.CC || true
fi

# Archlinux (which we use for testing various self-installed OpenSSL versions)
# stores their trust list under /etc/ca-certificates/extracted/.
# We need to copy it to our custom installed OpenSSL/LibreSSL trust store.
declare pem_file="/etc/ca-certificates/extracted/tls-ca-bundle.pem"
# LibreSSL bundle their own trust store (in install-dir/etc/ssl/cert.pem)
[[ ! -d "${install_dir}/ssl" ]] || cp "${pem_file}" "${install_dir}/ssl/cert.pem" || true
# OpenSSL fips enabled path
cp "${pem_file}" "${install_dir}/cert.pem" || true

export PATH
PATH="$(pwd)/install-dir/bin:${PATH:-}"

openssl version || true
ulimit -c unlimited || true

if [[ "${ssl}" != "OFF" ]]; then
  # FIXME: CDRIVER-2008
  if [[ "${OSTYPE}" != "cygwin" ]]; then
    echo "Authenticating using X.509"
    "${ping}" "mongodb://CN=client,OU=kerneluser,O=10Gen,L=New York City,ST=New York,C=US@${auth_host}/?ssl=true&authMechanism=MONGODB-X509&sslClientCertificateKeyFile=src/libmongoc/tests/x509gen/legacy-x509.pem&sslCertificateAuthorityFile=src/libmongoc/tests/x509gen/legacy-ca.crt&sslAllowInvalidHostnames=true&${c_timeout}"
  fi

  echo "Connecting to Atlas Free Tier"
  "${ping}" "${atlas_free:?}&${c_timeout}"
  echo "Connecting to Atlas Free Tier with SRV"
  "${ping}" "${atlas_free_srv:?}&${c_timeout}"
  echo "Connecting to Atlas Replica Set"
  "${ping}" "${atlas_replset:?}&${c_timeout}"
  echo "Connecting to Atlas Replica Set with SRV"
  "${ping}" "${atlas_replset_srv:?}${c_timeout}"
  echo "Connecting to Atlas Sharded Cluster"
  "${ping}" "${atlas_shard:?}&${c_timeout}"
  echo "Connecting to Atlas Sharded Cluster with SRV"
  "${ping}" "${atlas_shard_srv:?}${c_timeout}"
  if [[ -z "${require_tls12:-}" ]]; then
    echo "Connecting to Atlas with only TLS 1.1 enabled"
    "${ping}" "${atlas_tls11:?}&${c_timeout}"
    echo "Connecting to Atlas with only TLS 1.1 enabled with SRV"
    "${ping}" "${atlas_tls11_srv:?}${c_timeout}"
  fi
  echo "Connecting to Atlas with only TLS 1.2 enabled"
  "${ping}" "${atlas_tls12:?}&${c_timeout}"
  echo "Connecting to Atlas with only TLS 1.2 enabled with SRV"
  "${ping}" "${atlas_tls12_srv:?}${c_timeout}"
  echo "Connecting to Atlas Serverless with SRV"
  "${ping}" "${atlas_serverless_srv:?}/?${c_timeout}"
  echo "Connecting to Atlas Serverless"
  "${ping}" "${atlas_serverless:?}&${c_timeout}"
fi

echo "Authenticating using PLAIN"
"${ping}" "mongodb://${auth_plain:?}@${auth_host}/?authMechanism=PLAIN&${c_timeout}"

echo "Authenticating using default auth mechanism"
"${ping}" "mongodb://${auth_mongodbcr:?}@${auth_host}/mongodb-cr?${c_timeout}"

if [[ "${sasl}" != "OFF" ]]; then
  echo "Authenticating using GSSAPI"
  "${ping}" "mongodb://${auth_gssapi:?}@${auth_host}/?authMechanism=GSSAPI&${c_timeout}"

  echo "Authenticating with CANONICALIZE_HOST_NAME"
  "${ping}" "mongodb://${auth_gssapi:?}@${ip_addr}/?authMechanism=GSSAPI&authMechanismProperties=CANONICALIZE_HOST_NAME:true&${c_timeout}"

  declare ld_preload="${LD_PRELOAD:-}"
  if [[ "${ASAN}" == "on" ]]; then
    ld_preload="$(bypass_dlclose):${ld_preload}"
  fi

  echo "Test threaded GSSAPI auth"
  MONGOC_TEST_GSSAPI_HOST="${auth_host}" MONGOC_TEST_GSSAPI_USER="${auth_gssapi}" LD_PRELOAD="${ld_preload:-}" "${test_gssapi}"
  echo "Threaded GSSAPI auth OK"

  if [[ "${OSTYPE}" == "cygwin" ]]; then
    echo "Authenticating using GSSAPI (service realm: LDAPTEST.10GEN.CC)"
    "${ping}" "mongodb://${auth_crossrealm:?}@${auth_host}/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC&${c_timeout}"
    echo "Authenticating using GSSAPI (UTF-8 credentials)"
    "${ping}" "mongodb://${auth_gssapi_utf8:?}@${auth_host}/?authMechanism=GSSAPI&${c_timeout}"
  fi
fi
