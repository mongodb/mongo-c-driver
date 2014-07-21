#!/usr/bin/env bash


#
# This is a script to build mongo-c-driver on a few different platforms.
# It is suitable for execution by an automated build system such as buildbot.
#



# Check that we are in the top-level directory.
if [ ! -e "build/version.in" ]; then
	echo "Please run this from the top-level directory of mongo-c-driver."
	exit 1
fi

# Handle crazy path issues in Solaris
if [ "$(uname)" = "SunOS" -a -e "/etc/release" ]; then
	PATH=/usr/sbin:/usr/bin:/usr/ccs/bin:/usr/sfw/bin:/opt/csw/bin
fi

GIT=$(which git)
OS=$(uname -s)
ARCH=$(uname -p)
DISTRIB="unknown"
LSB_RELEASE="$(which lsb_release 2>/dev/null)"
GMAKE=$(which gmake)
MKDIR=$(which mkdir)
PERL=$(which perl)

RUN_TESTS=yes

if [ "$1" = "--notest" ]; then
	RUN_TESTS=no
	export DEB_BUILD_OPTIONS="$DEB_BUILD_OPTIONS nocheck"
fi

function make_check() {
	if [ "${RUN_TESTS}" = "yes" ]; then
		${GMAKE} ${MAKEARGS} check
	fi
}

# Try to get the OS distributor such as "Fedora"
if [ "${OS}" = "Linux" -a -n "${LSB_RELEASE}" ]; then
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
	SunOS-*-*)
		if [ "${ARCH}" = "sparc" ]; then
			ALT_ARCH="sparcv9"
		fi

		if [ "${ARCH}" = "i86pc" ]; then
			ALT_ARCH="amd64"
		fi



		export CC="cc"
		export SASL_CFLAGS="-I/usr/include"
		export SASL_LIBS="-L/usr/lib -R/usr/lib -lsasl"

		rm -rf _install

		${MKDIR} -p _install

		export CFLAGS="-m32"
		export PKG_CONFIG_PATH=/usr/lib/pkgconfig
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${HARDEN} ${OPTIMIZE} --prefix=${PWD}/_install/usr --libdir=${PWD}/_install/usr/lib
		${GMAKE} ${MAKEARGS} all
		make_check
		${GMAKE} ${MAKEARGS} install
		${GMAKE} ${MAKEARGS} clean

		export SASL_CFLAGS="-I/usr/include"
		export SASL_LIBS="-L/usr/lib/${ALT_ARCH} -R/usr/lib/${ALT_ARCH} -lsasl"
		export CFLAGS="-m64"
		export PKG_CONFIG_PATH=/usr/lib/${ALT_ARCH}/pkgconfig
		./configure ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${HARDEN} ${OPTIMIZE} --prefix=${PWD}/_install/usr --libdir=${PWD}/_install/usr/lib/${ALT_ARCH}
		${GMAKE} ${MAKEARGS} all
		make_check
		${GMAKE} ${MAKEARGS} install

		VERSION=$(cat build/version)

		cd _install
		${PERL} -p -i -e "s#${PWD}##g" usr/lib/pkgconfig/*.pc usr/lib/${ALT_ARCH}/pkgconfig/*.pc
		echo "i pkginfo" > Prototype
		find usr/ -type f | pkgproto >> Prototype
		find usr/ -type l | pkgproto >> Prototype
		${PERL} -p -i -e "s# ${USER}# root#g" Prototype
		${PERL} -p -i -e "s# other# root#g" Prototype

		PSTAMP=$(date +%d%b%y)

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

		pkgmk -o -r / -b ${PWD}/_install/ -d ${PWD}/_install/ -f _install/Prototype

		if [ $? -eq 0 ]; then
			cd _install
			OUTFILE="../MONGOmongo-c-driver.${VERSION}.${ARCH}.pkg.tar.gz"
			tar -cf - MONGOmongo-c-driver | gzip -9 -c > ${OUTFILE}
			cd -
		fi

		;;


	Linux-*-Fedora|Linux-*-RedHat*|Linux-*-CentOS|Linux-*-SUSE*)
		# Clean up any legacy tarballs.
		rm -rf mongo-c-driver-*.tar.gz

		# Bootstrap, build, and run unit tests.
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HARDEN} ${OPTIMIZE}
		${GMAKE} ${MAKEARGS} all
		make_check

		# Extract the major.minor.micro triplet.
		VERSION="$(cat build/version)"

		# Build the libbson and mongo-c-driver RPM packages.
		${GMAKE} ${MAKEARGS} dist-gzip

		if [ "${DISTRIB}" = "SUSE LINUX" ]; then
			mkdir -p /usr/src/packages/SOURCES
			cp mongo-c-driver-*.tar.gz /usr/src/packages/SOURCES
		else
			cp mongo-c-driver-*.tar.gz ~/rpmbuild/SOURCES
		fi

		rpmbuild -bb build/rpm/mongo-c-driver.spec

		;;

	Linux-*-Debian|Linux-*-Ubuntu)
		DPKG_BUILDPACKAGE=$(which dpkg-buildpackage 2>/dev/null)
		if [ -z "${DPKG_BUILDPACKAGE}" ]; then
			echo "Please install dpkg-dev."
			exit 1
		fi

		if [ ! -e /usr/share/cdbs/1/class/autotools.mk ]; then
			echo "Please install cdbs, debhelper, and build-essential"
			exit 1
		fi

		python -c 'import lxml.etree'
		if [ $? -ne 0 ]; then
			echo "Please install python-lxml"
			exit 1
		fi

		# Bootstrap, build, and run unit tests.
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${MAN} ${HARDEN} ${OPTIMIZE}

		dpkg-buildpackage
		if [ $? -ne 0 ]; then
			exit 1
		fi

		;;

	*)
		./autogen.sh ${STATIC} ${VERBOSE} ${DEBUG} ${SSL} ${SASL} ${HARDEN} ${OPTIMIZE}
		${GMAKE} ${MAKEARGS} all
		make_check
		;;
esac

exit 0
