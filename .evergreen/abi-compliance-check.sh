#!/bin/bash
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
python build/calc_release_version.py > VERSION_CURRENT
python build/calc_release_version.py -p > VERSION_RELEASED
sh .evergreen/compile.sh
make install

# checkout the newest release
newest=`cat VERSION_RELEASED`
current=`cat VERSION_CURRENT`

git checkout tags/$newest -f

# build the newest release
export SKIP_TESTS=ON
export EXTRA_CONFIGURE_FLAGS="-DCMAKE_INSTALL_PREFIX=./abi-compliance/latest-release-install -DCMAKE_C_FLAGS=-g -Og"
sh .evergreen/compile.sh
make install

cd abi-compliance

old_xml="<version>$newest</version>\n"
old_xml="${old_xml}<headers>\n"
old_xml="${old_xml}$(pwd)/latest-release-install/include/libmongoc-1.0/mongoc/mongoc.h\n"
old_xml="${old_xml}$(pwd)/latest-release-install/include/libbson-1.0/bson/bson.h\n"
old_xml="${old_xml}</headers>\n"
old_xml="${old_xml}<libs>$(pwd)/latest-release-install/lib</libs>"

printf $old_xml > old.xml

new_xml="<version>$current</version>\n"
new_xml="${new_xml}<headers>\n"
new_xml="${new_xml}$(pwd)/changes-install/include/libmongoc-1.0/mongoc/mongoc.h\n"
new_xml="${new_xml}$(pwd)/changes-install/include/libbson-1.0/bson/bson.h\n"
new_xml="${new_xml}</headers>\n"
new_xml="${new_xml}<libs>$(pwd)/changes-install/lib</libs>"

printf $new_xml > new.xml

# check for abi compliance. Generates HTML Reports
abi-compliance-checker -lib mongo-c-driver -old old.xml -new new.xml || result=$?
if [ -n "$result" ]; then
   touch ./abi-error.txt
fi
