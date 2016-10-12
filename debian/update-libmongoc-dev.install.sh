#!/bin/bash

ls -1 src/mongoc/*.{h,def,defs} \
  | xargs -n 1 basename \
  | sort \
  | awk '$0="usr/include/libmongoc-1.0/"$0' > debian/libmongoc-dev.install

echo 'usr/include/libmongoc-1.0/mongoc-version.h' >> debian/libmongoc-dev.install
echo 'usr/include/libmongoc-1.0/mongoc-config.h' >> debian/libmongoc-dev.install
echo 'usr/lib/*/libmongoc-[0-9]*.so' >> debian/libmongoc-dev.install
echo 'usr/lib/*/pkgconfig/libmongoc-[0-9]*' >> debian/libmongoc-dev.install
echo 'usr/lib/*/pkgconfig/libmongoc-ssl-[0-9]*' >> debian/libmongoc-dev.install
