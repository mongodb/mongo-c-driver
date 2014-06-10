#!/usr/bin/env bash

#
# This is a script to build mongo-c-driver on a few different platforms.
# It is suitable for execution by an automated build system such as buildbot.
#

GIT=$(which git)
OS=$(uname -s)
ARCH=$(uname -m)
DISTRIB="unknown"
LSB_RELEASE="$(which lsb_release)"
GMAKE=$(which gmake)
MKDIR=$(which mkdir)
PERL=$(which perl)

# Try to get the OS distributor such as "Fedora"
if [ -n "${LSB_RELEASE}" ]; then
	DISTRIB="$(${LSB_RELEASE} -i -s)"
fi

# Check for the location of the git binary.
if [ -z "${GIT}" ]; then
	echo "Please install git or update PATH."
	exit 1
fi

# Check for gmake, with special case for OS X.
if [ -z "${GMAKE}" ]; then
	GMAKE=$(which make)
	if [ -z "${GMAKE}" ]; then
		echo "Please install gmake/make or update PATH."
		exit 1
	fi
fi

# Check for mkdir
if [ -z "${MKDIR}" ]; then
	echo "Please update path to include 'mkdir'"
	exit 1
fi

# Set the address of the test mongod.
if [ -z "${MONGOC_TEST_HOST}" ]; then
	export MONGOC_TEST_HOST="127.0.0.1:27017"
fi

# Setup some convenience variables for configure options.
STATIC="--disable-static"
VERBOSE="--disable-silent-rules"
DEBUG="--enable-debug --enable-debug-symbols --enable-maintainer-flags"
SSL="--enable-ssl"
SASL="--enable-sasl"
MAN="--enable-man-pages"
HTML="--enable-html-docs"
HARDEN="--enable-hardening"
OPTIMIZE="--enable-optimizations"

# Per-build-system build commands.
case "${OS}-${ARCH}-${DISTRIB}" in
	SunOS-sparc-*)
		export CC="cc"
		export SASL_CFLAGS="-I/usr/include"
		export SASL_LIBS="-L/usr/lib -R/usr/lib -lsasl"

		${MKDIR} -p _install

		export CFLAGS="-m32"
		export PKG_CONFIG_PATH=/usr/lib/pkgconfig
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HARDEN} ${OPTIMIZE} --prefix=_install --libdir=_install/lib
		${GMAKE} ${MAKEARGS} all
		${GMAKE} ${MAKEARGS} check
		${GMAKE} ${MAKEARGS} install

		export CFLAGS="-m64"
		export PKG_CONFIG_PATH=/usr/lib/sparcv9/pkgconfig
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HARDEN} ${OPTIMIZE} --prefix=_install --libdir=_install/lib/sparcv9
		${GMAKE} ${MAKEARGS} all
		${GMAKE} ${MAKEARGS} check
		${GMAKE} ${MAKEARGS} install

		cd _install
		${PERL} -p -i -e "s#_install##g" usr/lib/pkgconfig/*.pc usr/lib/sparcv9/pkgconfig/*.pc
		echo "i pkginfo" > Prototype
		find usr/ -type f | pkgproto >> Prototype
		find usr/ -type l | pkgproto >> Prototype
		${PERL} -p -i -e "s# ${USER}# root#g" Prototype
		${PERL} -p -i -e "s# other# root#g" Prototype

		PSTAMP=$(date +%d%b%y)
		VERSION=$(cat build/version)

		cat <<EOF > pkginfo
PKG="MONGOmongo-c-driver"
NAME="MONGO mongo-c-driver ${VERSION}"
VERSION="${VERSION}"
ARCH="${ARCH}"
CLASSES="none"
CATEGORY="system"
VENDOR="MONGO"
PSTAMP="${PSTAMP}"
EMAIL="packaging@mongodb.com"
ISTATES="S s 1 2 3"
RSTATES="S s 1 2 3"
BASEDIR="/"
EOF

		cd -

		pkgmk -o -r / -b _install/ -d _install/ -f Prototype

		if [ $? -eq 0 ]; then
			tar -cf - MONGOmongo-c-driver | gzip -9 -c > MONGOmongo-c-driver.
		fi

		;;


	Linux-x86_64-Fedora)
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HTML} ${HARDEN} ${OPTIMIZE}
		${GMAKE} ${MAKEARGS} all
		${GMAKE} ${MAKEARGS} distcheck
		;;


	*)
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${HARDEN} ${OPTIMIZE}
		${GMAKE} ${MAKEARGS} all
		${GMAKE} ${MAKEARGS} check
		;;
esac
