#!/bin/sh
set -o xtrace   # Write all commands first to stderr
set -o errexit  # Exit the script with error if any of the commands fail

if [ ! -f resolv_wrapper_build/src/libresolv_wrapper.so ]; then
  curl -LO https://ftp.samba.org/pub/cwrap/resolv_wrapper-1.1.5.tar.gz
  tar xvf resolv_wrapper-1.1.5.tar.gz
  rm -rf resolv_wrapper_build
  mkdir resolv_wrapper_build
  cd resolv_wrapper_build
  PATH=$PATH:/opt/cmake/bin
  cmake ../resolv_wrapper-1.1.5
  make
  cd ..
fi

cat << EOF > hosts.txt
SRV _mongodb._tcp.test1.test.mongodb.com localhost 27017
SRV _mongodb._tcp.test1.test.mongodb.com localhost 27018
SRV _mongodb._tcp.test2.test.mongodb.com localhost 27018
SRV _mongodb._tcp.test2.test.mongodb.com localhost 27019
SRV _mongodb._tcp.test3.test.mongodb.com localhost 27017
EOF

export LD_PRELOAD=`pwd`/resolv_wrapper_build/src/libresolv_wrapper.so
export RESOLV_WRAPPER_HOSTS=hosts.txt
export MONGOC_TEST_SRV=on

./autogen.sh
make -j8
./test-libmongoc --no-fork -d -F test-results.json -l '/srv*'
