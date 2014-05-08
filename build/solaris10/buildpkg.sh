#!/usr/bin/bash

VENDOR=MONGO
EMAIL=packaging@mongodb.com

#
# This script will extract and build a Solaris SysV Revision 4 package as can
# be found on Solaris 10. This is not the same as Solaris 11, which uses the
# IPS packaging system.
#

export PATH="$PATH:/usr/ccs/bin:/usr/sfw/bin"

#
# Run with buildpkg.sh mongo-c-driver-ver.tar.gz mongo-c-driver 1.2.3 ARCH
# 
# ARCH can be i386 or sparc.
# Both 32-bit and 64-bit versions will be compiled for the host arch.
#

TARFILE="${PWD}/$1"
if [ -z "${TARFILE}" ]; then
    echo "usage: buildpkg.sh <tarfile> <name> <version> <arch>"
    exit 1
fi

NAME=$2
if [ -z "${NAME}" ]; then
    echo "usage: buildpkg.sh <tarfile> <name> <version> <arch>"
    exit 1
fi

VERSION=$3
if [ -z "${VERSION}" ]; then
    echo "usage: buildpkg.sh <tarfile> <name> <version> <arch>"
    exit 1
fi

# TODO: Detect host, compile for both 32 and 64.
ARCH=$4
if [ -z "${ARCH}" ]; then
    echo "usage: buildpkg.sh <tarfile> <name> <version> <arch>"
    exit 1
fi

if [ "${ARCH}" = "i386" ]; then
    ARCH64="amd64"
fi

if [ "${ARCH}" = "sparc" ]; then
    ARCH64="sparcv9"
fi

if [ -z "${ARCH64}" ]; then
    echo "arch must be either i386 or sparc"
    exit 1
fi

DIR=$(mktemp -d -p /tmp mongo-c-driver-XXXXXX)

function cleanup() {
    echo "Removing temporary directory ${DIR}"
    rm -rf "${DIR}"
}

#trap cleanup EXIT


cd "${DIR}"
cp "${TARFILE}" .
gunzip -c "$1" | tar -xf -
cd ${NAME}-${VERSION}


export CC=cc


#
# Build for 32-bit systems
#
export SASL_CFLAGS="-I/usr/include"
export SASL_LIBS="-L/usr/lib -lsasl"
export CFLAGS="-m32"
./configure "--prefix=${DIR}/usr" "--libdir=${DIR}/usr/lib" --enable-debug-symbols --enable-sasl --enable-ssl --disable-static --disable-silent-rules --enable-maintainer-flags --with-libbson=bundled
make
make install
make clean

#
# Build for 64-bit systems
#
export PKG_CONFIG_PATH="/usr/lib/${ARCH64}/pkgconfig"
export SASL_CFLAGS="-I/usr/include"
export SASL_LIBS="-L/usr/lib/${ARCH64} -lsasl"
export CFLAGS="-m64"
./configure "--prefix=${DIR}/usr" "--libdir=${DIR}/usr/lib/${ARCH64}" --enable-debug-symbols --enable-sasl --enable-ssl --disable-static --disable-silent-rules --enable-maintainer-flags --with-libbson=bundled
make
make install


#
# Find all files that belong in the package.
#
cd "${DIR}"
perl -p -i -e "s#${DIR}##g" usr/lib/pkgconfig/*.pc usr/lib/${ARCH64}/pkgconfig/*.pc
echo "i pkginfo" > Prototype
find "usr/" | grep -v bin$ | grep -v lib$ | grep -v include$ | grep -v share$ | grep -v doc$ | grep -v usr/$ | grep -v amd64$ | grep -v sparcv9$ | grep -v pkgconfig$ | pkgproto >> Prototype
PSTAMP=$(date +%d%b%y)

cat <<EOF > pkginfo
PKG="${VENDOR}${NAME}"
NAME="${VENDOR} ${NAME} ${VERSION}"
VERSION="${VERSION}"
ARCH="${ARCH}"
CLASSES="none"
CATEGORY="system"
VENDOR="${VENDOR}"
PSTAMP="${PSTAMP}"
EMAIL="${EMAIL}"
ISTATES="S s 1 2 3"
RSTATES="S s 1 2 3"
BASEDIR="/"

EOF


pkgmk -o -r / -b "${DIR}" -d "${DIR}" -f Prototype

tar -cf - "${VENDOR}${NAME}" | gzip -9 -c > "${VENDOR}${NAME}.${VERSION}.${ARCH}.pkg.tar.gz"



