#!/bin/sh

#
# Test libmongoc's Debian packaging scripts.
#
# Supported/used environment variables:
#   IS_PATCH    If "true", this is an Evergreen patch build.

set -o xtrace
set -o errexit

if [ "${IS_PATCH}" = "true" ]; then
  git diff HEAD -- . ':!debian' > ../upstream.patch
  git diff HEAD -- debian > ../debian.patch
  git clean -fdx
  git reset --hard HEAD
  if [ -s ../upstream.patch ]; then
    [ -d debian/patches ] || mkdir debian/patches
    mv ../upstream.patch debian/patches/
    echo upstream.patch >> debian/patches/series
    git add debian/patches/*
    git commit -m 'Evergreen patch build - upstream changes'
    git log -n1 -p
  fi
  if [ -s ../debian.patch ]; then
    git apply --index ../debian.patch
    git commit -m 'Evergreen patch build - Debian packaging changes'
    git log -n1 -p
  fi
fi

cd ..

git clone https://salsa.debian.org/installer-team/debootstrap.git debootstrap.git
mkdir unstable-chroot
export DEBOOTSTRAP_DIR=`pwd`/debootstrap.git
# perl-openssl-defaults is explicitly listed to work around https://bugs.debian.org/907015
sudo -E ./debootstrap.git/debootstrap --include=build-essential,perl-openssl-defaults,git-buildpackage,fakeroot,debhelper,cmake,libssl-dev,pkg-config,python3-sphinx,zlib1g-dev,libicu-dev,libsasl2-dev,libsnappy-dev,python-git unstable ./unstable-chroot/ http://cdn-aws.deb.debian.org/debian
cp -a mongoc ./unstable-chroot/tmp/
sudo chroot ./unstable-chroot /bin/bash -c "(set -o xtrace && \
  cd /tmp/mongoc && \
  git clean -fdx && \
  git reset --hard HEAD && \
  python build/calc_release_version.py > VERSION_CURRENT && \
  python build/calc_release_version.py -p > VERSION_RELEASED && \
  git add --force VERSION_CURRENT VERSION_RELEASED && \
  git commit VERSION_CURRENT VERSION_RELEASED -m 'Set current/released versions' && \
  LANG=C /bin/bash ./debian/build_snapshot.sh && \
  debc ../*.changes && \
  dpkg -i ../*.deb && \
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0 )"

[ -e ./unstable-chroot/tmp/mongoc/example-client ] || (echo "Example was not built!" ; exit 1)
(cd ./unstable-chroot/tmp/ ; tar zcvf ../../deb.tar.gz *.dsc *.orig.tar.gz *.debian.tar.xz *.build *.deb)
