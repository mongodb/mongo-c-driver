#!/bin/sh

#
# Test libmongoc's Debian packaging scripts.
#
# Supported/used environment variables:
#   IS_PATCH    If "true", this is an Evergreen patch build.

set -o errexit

on_exit () {
  if [ -e ./unstable-chroot/debootstrap/debootstrap.log ]; then
    echo "Dumping debootstrap.log (64-bit)"
    cat ./unstable-chroot/debootstrap/debootstrap.log
  fi
  if [ -e ./unstable-i386-chroot/debootstrap/debootstrap.log ]; then
    echo "Dumping debootstrap.log (32-bit)"
    cat ./unstable-i386-chroot/debootstrap/debootstrap.log
  fi
}
trap on_exit EXIT

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
export DEBOOTSTRAP_DIR=`pwd`/debootstrap.git
sudo -E ./debootstrap.git/debootstrap unstable ./unstable-chroot/ http://cdn-aws.deb.debian.org/debian
cp -a mongoc ./unstable-chroot/tmp/
sudo chroot ./unstable-chroot /bin/bash -c "(\
  apt-get install -y build-essential git-buildpackage fakeroot debhelper cmake libssl-dev pkg-config python3-sphinx python3-sphinx-design zlib1g-dev libsasl2-dev libsnappy-dev libzstd-dev libmongocrypt-dev libjs-mathjax libutf8proc-dev furo && \
  chown -R root:root /tmp/mongoc && \
  cd /tmp/mongoc && \
  git clean -fdx && \
  git reset --hard HEAD && \
  python3 build/calc_release_version.py > VERSION_CURRENT && \
  python3 build/calc_release_version.py -p > VERSION_RELEASED && \
  git add --force VERSION_CURRENT VERSION_RELEASED && \
  git commit VERSION_CURRENT VERSION_RELEASED -m 'Set current/released versions' && \
  LANG=C /bin/bash ./debian/build_snapshot.sh && \
  debc ../*.changes && \
  dpkg -i ../*.deb && \
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0 )"

[ -e ./unstable-chroot/tmp/mongoc/example-client ] || (echo "Example was not built!" ; exit 1)
(cd ./unstable-chroot/tmp/ ; tar zcvf ../../deb.tar.gz *.dsc *.orig.tar.gz *.debian.tar.xz *.build *.deb)

# Build a second time, to ensure a "double build" works
sudo chroot ./unstable-chroot /bin/bash -c "(\
  cd /tmp/mongoc && \
  rm -f example-client && \
  git status --ignored && \
  dpkg-buildpackage -b && dpkg-buildpackage -S )"

# And now do it all again for 32-bit
sudo -E ./debootstrap.git/debootstrap --arch i386 unstable ./unstable-i386-chroot/ http://cdn-aws.deb.debian.org/debian
cp -a mongoc ./unstable-i386-chroot/tmp/
sudo chroot ./unstable-i386-chroot /bin/bash -c "(\
  apt-get install -y build-essential git-buildpackage fakeroot debhelper cmake libssl-dev pkg-config python3-sphinx python3-sphinx-design zlib1g-dev libsasl2-dev libsnappy-dev libzstd-dev libmongocrypt-dev libjs-mathjax libutf8proc-dev furo && \
  chown -R root:root /tmp/mongoc && \
  cd /tmp/mongoc && \
  git clean -fdx && \
  git reset --hard HEAD && \
  python3 build/calc_release_version.py > VERSION_CURRENT && \
  python3 build/calc_release_version.py -p > VERSION_RELEASED && \
  git add --force VERSION_CURRENT VERSION_RELEASED && \
  git commit VERSION_CURRENT VERSION_RELEASED -m 'Set current/released versions' && \
  LANG=C /bin/bash ./debian/build_snapshot.sh && \
  debc ../*.changes && \
  dpkg -i ../*.deb && \
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0 )"

[ -e ./unstable-i386-chroot/tmp/mongoc/example-client ] || (echo "Example was not built!" ; exit 1)
(cd ./unstable-i386-chroot/tmp/ ; tar zcvf ../../deb-i386.tar.gz *.dsc *.orig.tar.gz *.debian.tar.xz *.build *.deb)

# Build a second time, to ensure a "double build" works
sudo chroot ./unstable-i386-chroot /bin/bash -c "(\
  cd /tmp/mongoc && \
  rm -f example-client && \
  git status --ignored && \
  dpkg-buildpackage -b && dpkg-buildpackage -S )"
