#!/bin/sh

#
# Test libmongoc's Debian packaging scripts.
#
# Supported/used environment variables:
#   IS_PATCH    If "true", this is an Evergreen patch build.

set -o errexit

on_exit () {
  if [ -e ./trixie-chroot/debootstrap/debootstrap.log ]; then
    echo "Dumping debootstrap.log (64-bit)"
    cat ./trixie-chroot/debootstrap/debootstrap.log
  fi
  if [ -e ./trixie-i386-chroot/debootstrap/debootstrap.log ]; then
    echo "Dumping debootstrap.log (32-bit)"
    cat ./trixie-i386-chroot/debootstrap/debootstrap.log
  fi
}
trap on_exit EXIT

git config user.email "evergreen-build@example.com"
git config user.name "Evergreen Build"

# Note that here, on the r1.30 branch, the tag debian/1.30.4-1 is used rather
# than the branch debian/trixie. This is because the branch has advanced and
# now contains updated packaging for the 2.x releases.

if [ "${IS_PATCH}" = "true" ]; then
  git diff HEAD > ../upstream.patch
  git clean -fdx
  git reset --hard HEAD
  git remote add upstream https://github.com/mongodb/mongo-c-driver
  git fetch upstream
  CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  git checkout debian/1.30.4-1
  git checkout ${CURRENT_BRANCH}
  git checkout debian/1.30.4-1 -- ./debian/
  if [ -s ../upstream.patch ]; then
    [ -d debian/patches ] || mkdir debian/patches
    mv ../upstream.patch debian/patches/
    echo upstream.patch >> debian/patches/series
    git add debian/patches/*
    git commit -m 'Evergreen patch build - upstream changes'
    git log -n1 -p
  fi
fi

cd ..

git clone https://salsa.debian.org/installer-team/debootstrap.git debootstrap.git
export DEBOOTSTRAP_DIR=`pwd`/debootstrap.git
sudo -E ./debootstrap.git/debootstrap --variant=buildd trixie ./trixie-chroot/ http://cdn-aws.deb.debian.org/debian
cp -a mongoc ./trixie-chroot/tmp/
sudo chroot ./trixie-chroot /bin/bash -c '(\
  apt-get install -y build-essential git-buildpackage fakeroot dpkg-dev debhelper cmake libssl-dev pkgconf python3-sphinx python3-sphinx-design furo libmongocrypt-dev zlib1g-dev libsasl2-dev libsnappy-dev libutf8proc-dev libzstd-dev libjs-mathjax python3-packaging && \
  chown -R root:root /tmp/mongoc && \
  cd /tmp/mongoc && \
  git clean -fdx && \
  git reset --hard HEAD && \
  git remote remove upstream || true && \
  git remote add upstream https://github.com/mongodb/mongo-c-driver && \
  git fetch upstream && \
  export CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)" && \
  git checkout debian/1.30.4-1 && \
  git checkout ${CURRENT_BRANCH} && \
  git checkout debian/1.30.4-1 -- ./debian/ && \
  git commit -m "fetch debian directory from the debian/unstable branch" && \
  LANG=C /bin/bash ./debian/build_snapshot.sh && \
  debc ../*.changes && \
  dpkg -i ../*.deb && \
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0 )'

[ -e ./trixie-chroot/tmp/mongoc/example-client ] || (echo "Example was not built!" ; exit 1)
(cd ./trixie-chroot/tmp/ ; tar zcvf ../../deb.tar.gz *.dsc *.orig.tar.gz *.debian.tar.xz *.build *.deb)

# Build a second time, to ensure a "double build" works
sudo chroot ./trixie-chroot /bin/bash -c "(\
  cd /tmp/mongoc && \
  rm -f example-client && \
  git status --ignored && \
  dpkg-buildpackage -b && dpkg-buildpackage -S )"

# And now do it all again for 32-bit
sudo -E ./debootstrap.git/debootstrap --variant=buildd --arch i386 trixie ./trixie-i386-chroot/ http://cdn-aws.deb.debian.org/debian
cp -a mongoc ./trixie-i386-chroot/tmp/
sudo chroot ./trixie-i386-chroot /bin/bash -c '(\
  apt-get install -y build-essential git-buildpackage fakeroot dpkg-dev debhelper cmake libssl-dev pkgconf python3-sphinx python3-sphinx-design furo libmongocrypt-dev zlib1g-dev libsasl2-dev libsnappy-dev libutf8proc-dev libzstd-dev libjs-mathjax && \
  chown -R root:root /tmp/mongoc && \
  cd /tmp/mongoc && \
  git clean -fdx && \
  git reset --hard HEAD && \
  git remote remove upstream || true && \
  git remote add upstream https://github.com/mongodb/mongo-c-driver && \
  git fetch upstream && \
  export CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)" && \
  git checkout debian/1.30.4-1 && \
  git checkout ${CURRENT_BRANCH} && \
  git checkout debian/1.30.4-1 -- ./debian/ && \
  git commit -m "fetch debian directory from the debian/unstable branch" && \
  LANG=C /bin/bash ./debian/build_snapshot.sh && \
  debc ../*.changes && \
  dpkg -i ../*.deb && \
  gcc -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0 -o example-client src/libmongoc/examples/example-client.c -lmongoc-1.0 -lbson-1.0 )'

[ -e ./trixie-i386-chroot/tmp/mongoc/example-client ] || (echo "Example was not built!" ; exit 1)
(cd ./trixie-i386-chroot/tmp/ ; tar zcvf ../../deb-i386.tar.gz *.dsc *.orig.tar.gz *.debian.tar.xz *.build *.deb)

# Build a second time, to ensure a "double build" works
sudo chroot ./trixie-i386-chroot /bin/bash -c "(\
  cd /tmp/mongoc && \
  rm -f example-client && \
  git status --ignored && \
  dpkg-buildpackage -b && dpkg-buildpackage -S )"
