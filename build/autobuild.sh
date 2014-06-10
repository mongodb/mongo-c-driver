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
	if [ $(uname -s) = "Darwin" ]; then
		GMAKE=make
	else
		echo "Please install gmake or update PATH."
	fi
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
		export CFLAGS="-m64"
		export CC="cc"
		export SASL_CFLAGS="-I/usr/include"
		export SASL_LIBS="-L/usr/lib -R/usr/lib -lsasl"
		export PKG_CONFIG_PATH=/usr/lib/sparcv9/pkgconfig
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HARDEN} ${OPTIMIZE}
		${GMAKE} ${MAKEARGS} all
		${GMAKE} ${MAKEARGS} check
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
