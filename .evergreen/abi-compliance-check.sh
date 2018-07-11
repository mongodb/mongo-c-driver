#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit

# create all needed directories
mkdir abi-compliance
mkdir abi-compliance/changes-install
mkdir abi-compliance/latest-release-install
mkdir abi-compliance/dumps

# build the current changes
export SKIP_TESTS=ON
export EXTRA_CONFIGURE_FLAGS="-DCMAKE_INSTALL_PREFIX=./abi-compliance/changes-install -DCMAKE_C_FLAGS=-g -Og"
sh .evergreen/compile.sh
make install

# checkout the newest release
newest=`cat VERSION_RELEASED`

# CDRIVER-2731: Update this line in verison 1.11.1 
git checkout tags/$newest -f -- src

# build the newest release
export SKIP_TESTS=ON
export EXTRA_CONFIGURE_FLAGS="-DCMAKE_INSTALL_PREFIX=./abi-compliance/latest-release-install -DCMAKE_C_FLAGS=-g -Og"
sh .evergreen/compile.sh
make install

cd abi-compliance

# create the abi dumps for libmongoc
abi-dumper ./changes-install/lib/libmongoc-1.0.so -o ./dumps/mongoc-changes.dump
abi-dumper ./latest-release-install/lib/libmongoc-1.0.so -o ./dumps/mongoc-release.dump

# create abi dumps for libbson
abi-dumper ./changes-install/lib/libbson-1.0.so -o ./dumps/bson-changes.dump
abi-dumper ./latest-release-install/lib/libbson-1.0.so -o ./dumps/bson-release.dump

# check libmongoc and libbson for compliance. Generates HTML Reports
abi-compliance-checker -l libmongoc -old ./dumps/mongoc-release.dump -new ./dumps/mongoc-changes.dump || result=$?
if [ -n "$result" ]; then
   touch ./abi-error.txt
fi
abi-compliance-checker -l libbson -old ./dumps/bson-release.dump -new ./dumps/bson-changes.dump || result=$?
if [ -n "$result" ]; then
   touch ./abi-error.txt
fi
