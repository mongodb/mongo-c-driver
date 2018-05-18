#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail
set +o xtrace   # Don't echo commands

# The following expansions are set in the evergreen project:
# AUTH_HOST=${auth_host} # Evergreen variable
# AUTH_PLAIN=${auth_plain} # Evergreen variable
# AUTH_MONGODBCR=${auth_mongodbcr} # Evergreen variable
# AUTH_GSSAPI=${auth_gssapi} # Evergreen variable
# AUTH_CROSSREALM=${auth_crossrealm} # Evergreen variable
# AUTH_GSSAPI_UTF8=${auth_gssapi_utf8} # Evergreen variable
# ATLAS_FREE=${atlas_free} # Evergreen variable
# ATLAS_REPLSET=${atlas_replset} # Evergreen variable
# ATLAS_SHARD=${atlas_shard} # Evergreen variable
# ATLAS_TLS11=${atlas_tls11} # Evergreen variable
# ATLAS_TLS12=${atlas_tls12} # Evergreen variable
# REQUIRE_TLS12=${require_tls12} # libmongoc requires TLS 1.2+
# OBSOLETE_TLS=${obsolete_tls} # libmongoc was built with old TLS lib, don't try connecting to Atlas


C_TIMEOUT="connectTimeoutMS=30000&serverSelectionTryOnce=false"


OS=$(uname -s | tr '[:upper:]' '[:lower:]')
if grep -q "#define MONGOC_ENABLE_SASL 1" src/libmongoc/src/mongoc/mongoc-config.h; then
   SASL=1
else
   SASL=0
fi
if grep -q "#define MONGOC_ENABLE_SSL 1" src/libmongoc/src/mongoc/mongoc-config.h; then
   SSL=1
else
   SSL=0
fi

DIR=$(dirname $0)
. $DIR/set-path.sh

case "$OS" in
   cygwin*)
      PING="./src/libmongoc/Debug/mongoc-ping.exe"
      TEST_GSSAPI="./src/libmongoc/Debug/test-mongoc-gssapi.exe"
      IP_ADDR=`getent hosts $AUTH_HOST | head -n 1 | awk '{print $1}'`
      ;;

   darwin)
      PING="./src/libmongoc/mongoc-ping"
      TEST_GSSAPI="./src/libmongoc/test-mongoc-gssapi"
      IP_ADDR=`dig $AUTH_HOST +short | tail -1`
      ;;

   *)
      PING="./src/libmongoc/mongoc-ping"
      TEST_GSSAPI="./src/libmongoc/test-mongoc-gssapi"
      IP_ADDR=`getent hosts $AUTH_HOST | head -n 1 | awk '{print $1}'`
esac

if test -f /tmp/drivers.keytab; then
   kinit -k -t /tmp/drivers.keytab -p drivers@LDAPTEST.10GEN.CC || true
fi

# Archlinux (which we use for testing various self-installed OpenSSL versions)
# Stores their trust list here. We need to copy it to our custom installed
# OpenSSL trust store.
# LibreSSL bundle their own trust store (in install-dir/etc/ssl/cert.pem)
cp /etc/ca-certificates/extracted/tls-ca-bundle.pem install-dir/ssl/cert.pem || true
# OpenSSL fips enabled path
cp /etc/ca-certificates/extracted/tls-ca-bundle.pem install-dir/cert.pem || true

export PATH=install-dir/bin:$PATH
openssl version || true
ulimit -c unlimited || true

if [ $SSL -eq 1 ]; then
   # FIXME: CDRIVER-2008
   if [ "${OS%_*}" != "cygwin" ]; then
      echo "Authenticating using X.509"
      $PING "mongodb://CN=client,OU=kerneluser,O=10Gen,L=New York City,ST=New York,C=US@${AUTH_HOST}/?ssl=true&authMechanism=MONGODB-X509&sslClientCertificateKeyFile=src/libmongoc/tests/x509gen/legacy-x509.pem&sslCertificateAuthorityFile=src/libmongoc/tests/x509gen/legacy-ca.crt&sslAllowInvalidHostnames=true&${C_TIMEOUT}"
   fi
   if [ "${OBSOLETE_TLS}" != "true" ]; then
      echo "Connecting to Atlas Free Tier"
      $PING "$ATLAS_FREE&${C_TIMEOUT}"
      echo "Connecting to Atlas Replica Set"
      $PING "$ATLAS_REPLSET&${C_TIMEOUT}"
      echo "Connecting to Atlas Sharded Cluster"
      $PING "$ATLAS_SHARD&${C_TIMEOUT}"
      if [ -z "$REQUIRE_TLS12" ]; then
         echo "Connecting to Atlas with only TLS 1.1 enabled"
         $PING "$ATLAS_TLS11&${C_TIMEOUT}"
      fi
      echo "Connecting to Atlas with only TLS 1.2 enabled"
      $PING "$ATLAS_TLS12&${C_TIMEOUT}"
   fi
fi

echo "Authenticating using PLAIN"
$PING "mongodb://${AUTH_PLAIN}@${AUTH_HOST}/?authMechanism=PLAIN&${C_TIMEOUT}"

echo "Authenticating using default auth mechanism"
$PING "mongodb://${AUTH_MONGODBCR}@${AUTH_HOST}/mongodb-cr?${C_TIMEOUT}"

if [ $SASL -eq 1 ]; then
   echo "Authenticating using GSSAPI"
   $PING "mongodb://${AUTH_GSSAPI}@${AUTH_HOST}/?authMechanism=GSSAPI&${C_TIMEOUT}"

   echo "Authenticating with CANONICALIZE_HOST_NAME"
   $PING "mongodb://${AUTH_GSSAPI}@${IP_ADDR}/?authMechanism=GSSAPI&authMechanismProperties=CANONICALIZE_HOST_NAME:true&${C_TIMEOUT}"

   echo "Test threaded GSSAPI auth"
   MONGOC_TEST_GSSAPI_HOST="${AUTH_HOST}" MONGOC_TEST_GSSAPI_USER="${AUTH_GSSAPI}" $TEST_GSSAPI
   echo "Threaded GSSAPI auth OK"

   if [ "${OS%_*}" = "cygwin" ]; then
      echo "Authenticating using GSSAPI (service realm: LDAPTEST.10GEN.CC)"
      $PING "mongodb://${AUTH_CROSSREALM}@${AUTH_HOST}/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC&${C_TIMEOUT}"
      echo "Authenticating using GSSAPI (UTF-8 credentials)"
      $PING "mongodb://${AUTH_GSSAPI_UTF8}@${AUTH_HOST}/?authMechanism=GSSAPI&${C_TIMEOUT}"
   fi
fi
