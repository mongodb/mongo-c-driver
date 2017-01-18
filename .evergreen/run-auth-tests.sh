#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail


OS=$(uname -s | tr '[:upper:]' '[:lower:]')

case "$OS" in
   cygwin*)
      export PATH=$PATH:`pwd`/tests:`pwd`/Debug:`pwd`/src/libbson/Debug
      chmod +x ./Debug/* src/libbson/Debug/*
      ./Debug/mongoc-ping.exe "mongodb://schrÃ¶dinger%40LDAPTEST.10GEN.CC:regnidÃ¶rhcs@ldaptest.10gen.cc/?authMechanism=GSSAPI"
      ./Debug/mongoc-ping.exe "mongodb://drivers%40LDAPTEST.10GEN.CC:powerbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI"
      ./Debug/mongoc-ping.exe "mongodb://drivers%40LDAPTEST2.10GEN.CC:weakbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC"
      ;;

   *)
      ./mongoc-ping "mongodb://schrÃ¶dinger%40LDAPTEST.10GEN.CC:regnidÃ¶rhcs@ldaptest.10gen.cc/?authMechanism=GSSAPI"
      ./mongoc-ping "mongodb://drivers%40LDAPTEST.10GEN.CC:powerbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI"
      ./mongoc-ping "mongodb://drivers%40LDAPTEST2.10GEN.CC:weakbook17@ldaptest.10gen.cc/?authMechanism=GSSAPI&authMechanismProperties=SERVICE_REALM:LDAPTEST.10GEN.CC"
      ;;
esac

