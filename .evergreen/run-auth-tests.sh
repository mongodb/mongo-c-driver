#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail
# AUTH_HOST=${auth_host} # Evergreen variable
# AUTH_PLAIN=${auth_plain} # Evergreen variable
# AUTH_MONGODBCR=${auth_mongodbcr} # Evergreen variable
# AUTH_GSSAPI=${auth_gssapi} # Evergreen variable
# AUTH_CROSSREALM=${auth_crossrealm} # Evergreen variable
# AUTH_GSSAPI_UTF8=${auth_gssapi_utf8} # Evergreen variable


OS=$(uname -s | tr '[:upper:]' '[:lower:]')
if grep -q "#define MONGOC_ENABLE_SASL 1" src/mongoc/mongoc-config.h; then
   SASL=1
else
   SASL=0
fi
if grep -q "#define MONGOC_ENABLE_SSL 1" src/mongoc/mongoc-config.h; then
   SSL=1
else
   SSL=0
fi

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
      chmod +x ./Debug/* src/libbson/Debug/*
      PING="./Debug/mongoc-ping.exe"
      ;;

   darwin)
      export DYLD_LIBRARY_PATH="install-dir/lib:.libs:src/libbson/.libs"
      PING="./mongoc-ping"
      ;;

   sunos)
      PATH="/opt/mongodbtoolchain/bin:$PATH"
      export LD_LIBRARY_PATH="install-dir/lib:/opt/csw/lib/amd64/:.libs:src/libbson/.libs"
      PING="./mongoc-ping"
      ;;

   *)
      # This libtool wrapper script was built in a unique dir like
      # "/data/mci/998e754a0d1ed79b8bf733f405b87778/mongoc",
      # replace its absolute path with "." so it can run in the CWD.
      sed -i'' 's/\/data\/mci\/[a-z0-9]\{32\}\/mongoc/./g' mongoc-ping
      export LD_LIBRARY_PATH="install-dir/lib:.libs:src/libbson/.libs"
      PING="./mongoc-ping"
esac

if test -f /tmp/drivers.keytab; then
   kinit -k -t /tmp/drivers.keytab -p drivers@LDAPTEST.10GEN.CC || true
fi
export PATH=install-dir/bin:$PATH
openssl version || true

if [ $SSL -eq 1 ]; then
   # FIXME: CDRIVER-2008
   if [ "${OS%_*}" != "cygwin" ]; then
      $PING "mongodb://CN=client,OU=kerneluser,O=10Gen,L=New York City,ST=New York,C=US@${AUTH_HOST}/?ssl=true&authMechanism=MONGODB-X509&sslClientCertificateKeyFile=./tests/x509gen/legacy-x509.pem&sslCertificateAuthorityFile=tests/x509gen/legacy-ca.crt&sslAllowInvalidHostnames=true"
   fi
fi

$PING "mongodb://${AUTH_PLAIN}@${AUTH_HOST}/?authMechanism=PLAIN"
$PING "mongodb://${AUTH_MONGODBCR}@${AUTH_HOST}/mongodb-cr?authMechanism=MONGODB-CR"

if [ $SASL -eq 1 ]; then
   $PING "mongodb://${AUTH_GSSAPI}@${AUTH_HOST}/?authMechanism=GSSAPI"
   if [ "${OS%_*}" = "cygwin" ]; then
      $PING "mongodb://${AUTH_CROSSREALM}@${AUTH_HOST}/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC"
      $PING "mongodb://${AUTH_GSSAPI_UTF8}@${AUTH_HOST}/?authMechanism=GSSAPI"
   fi
fi

