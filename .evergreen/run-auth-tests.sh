#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail


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
      # Evergreen fails apply a patchbuild with this file as binary
      # We therefore have the registry file encoded as base64, then decode it
      # before we load the registry changes
      # Removing \r is important as base64 decoding will otherwise fail!
      # When BUILD-2708 is fixed, we can remove these lines, and the file
      cat .evergreen/regedit.base64 | tr -d '\r' > tmp.base64
      base64 --decode tmp.base64 > .evergreen/kerberos.reg
      regedit.exe /S .evergreen/kerberos.reg
      PING="./Debug/mongoc-ping.exe"
      ;;

   darwin)
      export DYLD_LIBRARY_PATH=".libs:src/libbson/.libs"
      PING="./mongoc-ping"
      ;;

   sunos)
      PATH="/opt/mongodbtoolchain/bin:$PATH"
      export LD_LIBRARY_PATH="/opt/csw/lib/amd64/:.libs:src/libbson/.libs"
      PING="./mongoc-ping"
      ;;

   *)
      # This libtool wrapper script was built in a unique dir like
      # "/data/mci/998e754a0d1ed79b8bf733f405b87778/mongoc",
      # replace its absolute path with "." so it can run in the CWD.
      sed -i'' 's/\/data\/mci\/[a-z0-9]\{32\}\/mongoc/./g' mongoc-ping
      export LD_LIBRARY_PATH=".libs:src/libbson/.libs"
      PING="./mongoc-ping"
esac

if test -f /tmp/drivers.keytab; then
   kinit -k -t /tmp/drivers.keytab -p drivers@LDAPTEST.10GEN.CC || true
fi

$PING 'mongodb://drivers-team:mongor0x$xgen@ldaptest.10gen.cc/?authMechanism=PLAIN'
$PING 'mongodb://drivers:mongor0x$xgen@ldaptest.10gen.cc/mongodb-cr?authMechanism=MONGODB-CR'

if [ $SASL -eq 1 ]; then
   $PING "mongodb://drivers%40LDAPTEST.10GEN.CC:powerbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI"
   if [ "${OS%_*}" = "cygwin" ]; then
      $PING "mongodb://drivers%40LDAPTEST2.10GEN.CC:weakbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC"
      $PING "mongodb://schrÃ¶dinger%40LDAPTEST.10GEN.CC:regnidÃ¶rhcs@ldaptest.10gen.cc/?authMechanism=GSSAPI"
   fi
fi

