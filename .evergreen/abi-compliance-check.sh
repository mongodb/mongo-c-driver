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
current=`cat VERSION_CURRENT`

git checkout tags/$newest -f

# build the newest release
export SKIP_TESTS=ON
export EXTRA_CONFIGURE_FLAGS="-DCMAKE_INSTALL_PREFIX=./abi-compliance/latest-release-install -DCMAKE_C_FLAGS=-g -Og"
sh .evergreen/compile.sh
make install

cd abi-compliance

# create the xml files for the old version and the new version
echo "<version>$newest</version><headers>$(pwd)/latest-release-install/include</headers><libs>$(pwd)/latest-release-install/lib</libs>" > old.xml
echo "<version>$current</version><headers>$(pwd)/changes-install/include</headers><libs>$(pwd)/changes-install/lib</libs>" > new.xml

# check for abi compliance. Generates HTML Reports
abi-compliance-checker -lib mongo-c-driver -old old.xml -new new.xml || result=$?
if [ -n "$result" ]; then
   touch ./abi-error.txt
fi
