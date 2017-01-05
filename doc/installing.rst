:man_page: mongoc_installing

Installing the MongoDB C Driver
===============================

The following guide will step you through the process of downloading, building, and installing the current release of the MongoDB C Driver.

Supported Platforms
-------------------

The MongoDB C Driver is `continuously tested <https://evergreen.mongodb.com/waterfall/mongo-c-driver>`_ on variety of platforms including:

====================================================  ================================================================  ==============================================================================================================================================================================================================
                                                                                                                                                                                                                                                                                                                                      
* GNU/Linux
* Solaris
* Mac OS X
* Microsoft Windows  * x86 and x86_64
* ARM
* PPC
* SPARC
* aarch64
* s390x
* ppc64le  * GCC 4.1 and newer
* Clang 3.3 and newer
* Microsoft Visual Studio 2010 and newer
* `Oracle Solaris Studio 12 <http://www.oracle.com/technetwork/server-storage/solarisstudio/downloads/index.html>`_
* MinGW
====================================================  ================================================================  ==============================================================================================================================================================================================================

The driver is also known to work on FreeBSD, and should work on any POSIX compatible platform with a working c89 (or later) compiler.

Install with a Package Manager
------------------------------

The libmongoc package is available on recent versions of Debian and Ubuntu.

.. code-block:: none

  $ apt-get install libmongoc-1.0-0

On Fedora, a mongo-c-driver package is available in the default repositories and can be installed with:

.. code-block:: none

  $ dnf install mongo-c-driver

On recent Red Hat systems, such as CentOS and RHEL 7, a mongo-c-driver package
      is available in the `EPEL <https://fedoraproject.org/wiki/EPEL>`_ repository. To check
      version available, see `https://apps.fedoraproject.org/packages/mongo-c-driver <https://apps.fedoraproject.org/packages/mongo-c-driver>`_.
      The package can be installed with:

.. code-block:: none

  $ yum install mongo-c-driver

Building on Unix
----------------

Prerequisites
-------------

OpenSSL is required for authentication or for SSL connections to MongoDB. Kerberos or LDAP support requires Cyrus SASL.

To install all optional dependencies on RedHat / Fedora:

.. code-block:: none

  $ sudo yum install pkg-config openssl-devel cyrus-sasl-devel

On Debian / Ubuntu:

.. code-block:: none

  $ sudo apt-get install pkg-config libssl-dev libsasl2-dev

On FreeBSD:

.. code-block:: none

  $ su -c 'pkg install pkgconf openssl cyrus-sasl'

Building from a release tarball
-------------------------------

Unless you intend on contributing to the mongo-c-driver, you will want to build from a release tarball.

The most recent release of libmongoc is 1.4.0 and can be `downloaded here <https://github.com/mongodb/mongo-c-driver/releases/download/1.4.0/mongo-c-driver-1.4.0.tar.gz>`_. The following snippet will download and extract the driver, and configure it:

.. code-block:: none

  $ wget https://github.com/mongodb/mongo-c-driver/releases/download/1.4.0/mongo-c-driver-1.4.0.tar.gz
  $ tar xzf mongo-c-driver-1.4.0.tar.gz
  $ cd mongo-c-driver-1.4.0
  $ ./configure
      

If ``configure`` completed successfully, you'll see something like the following describing your build configuration.

.. code-block:: none

  libmongoc was configured with the following options:

  Build configuration:
  Enable debugging (slow)                          : no
  Compile with debug symbols (slow)                : no
  Enable GCC build optimization                    : yes
  Code coverage support                            : no
  Cross Compiling                                  : no
  Fast counters                                    : no
  SASL                                             : sasl2
  SSL                                              : yes
  Libbson                                          : bundled

  Documentation:
  Generate man pages                               : no
  Install man pages                                : no

mongo-c-driver contains a copy of libbson, in case your system does not already have libbson installed. The configure script will detect if libbson is not installed and use the bundled libbson.

.. code-block:: none

  $ make
  $ sudo make install
      

Building from git
-----------------

To build an unreleased version of the driver from git requires additional dependencies.

RedHat / Fedora:

.. code-block:: none

  $ sudo yum install git gcc automake autoconf libtool

Debian / Ubuntu:

.. code-block:: none

  $ sudo apt-get install git gcc automake autoconf libtool

FreeBSD:

.. code-block:: none

  $ su -c 'pkg install git gcc automake autoconf libtool'

Once you have the dependencies installed, clone the repository and build the current master or a particular release tag:

.. code-block:: none

  $ git clone https://github.com/mongodb/mongo-c-driver.git
  $ cd mongo-c-driver
  $ git checkout x.y.z  # To build a particular release
  $ ./autogen.sh --with-libbson=bundled
  $ make
  $ sudo make install
      

Generating the documentation
----------------------------

Install the ``yelp-tools`` and ``yelp-xsl`` packages, then:

.. code-block:: none

  $ ./configure --enable-html-docs --enable-man-pages
  $ make man html

Building on Mac OS X
--------------------

Prerequisites
-------------

XCode Command Line Tools
------------------------

To install the XCode Command Line Tools, just type ``xcode-select --install`` in the Terminal and follow the instructions.

OpenSSL support on El Capitan
-----------------------------

Beginning in OS X 10.11 El Capitan, OS X no longer includes the OpenSSL headers. To build the driver with SSL on El Capitan and later, first `install Homebrew according to its instructions <http://brew.sh/>`_, then:

.. code-block:: none

  $ brew install openssl
  $ export LDFLAGS="-L/usr/local/opt/openssl/lib"
  $ export CPPFLAGS="-I/usr/local/opt/openssl/include"

Native TLS Support on Mac OS X / Darwin (Secure Transport)
----------------------------------------------------------

	The MongoDB C Driver supports the Darwin native TLS and crypto libraries.
	Using the native libraries there is no need to install OpenSSL. By
	default however, the driver will compile against OpenSSL if it
	detects it being available. If OpenSSL is not available, it will
	fallback on the native libraries.
      

	To compile against the Darwin native TLS and crypto libraries, even when
	OpenSSL is available, configure the driver like so:
      

.. code-block:: none

  $ ./configure --enable-ssl=darwin
      

Building on OS X
----------------

Download the latest release tarball:

.. code-block:: none

  $ curl -LO https://github.com/mongodb/mongo-c-driver/releases/download/1.4.0/mongo-c-driver-1.4.0.tar.gz
  $ tar xzf mongo-c-driver-1.4.0.tar.gz
  $ cd mongo-c-driver-1.4.0

Build and install the driver:

.. code-block:: none

  $ ./configure
  $ make
  $ sudo make install

Generating the documentation on OS X
------------------------------------

Homebrew is required to generate the driver's HTML documentation and man pages:

.. code-block:: none

  $ brew install yelp-xsl yelp-tools
  $ ./configure --enable-html-docs --enable-man-pages
  $ make man html

Installing on Mac OS X
----------------------

To build the C Driver on a Mac, install the prerequisites in order to build it from source. It is recommended to use `Homebrew <http://brew.sh>`_:

.. code-block:: none

  $ brew install automake autoconf libtool pkgconfig

Additionally, `XCode <http://developer.apple.com/xcode>`_ is required. The driver can then be installed by following the directions for :ref:`building from source <installing_build_yourself>`.

Building on Windows
-------------------

Building on Windows requires Windows Vista or newer and Visual Studio 2010 or newer. Additionally, ``cmake`` is required to generate Visual Studio project files.

Let's start by generating Visual Studio project files for libbson, a dependency of the C driver. The following assumes we are compiling for 64-bit Windows using Visual Studio 2015 Express, which can be freely downloaded from Microsoft.

.. code-block:: none

  cd mongo-c-driver-1.4.0\src\libbson
  cmake -G "Visual Studio 14 2015 Win64" "-DCMAKE_INSTALL_PREFIX=C:\mongo-c-driver"

(Run ``cmake -LH .`` for a list of other options.)

Now that we have project files generated, we can either open the project in Visual Studio or compile from the command line. Let's build using the command line program ``msbuild.exe``

.. code-block:: none

  msbuild.exe ALL_BUILD.vcxproj

Now that libbson is compiled, let's install it using msbuild. It will be installed to the path specified by ``CMAKE_INSTALL_PREFIX``.

.. code-block:: none

  msbuild.exe INSTALL.vcxproj

You should now see libbson installed in ``C:\mongo-c-driver``

Now let's do the same for the MongoDB C driver.

.. code-block:: none

  cd mongo-c-driver-1.4.0
  cmake -G "Visual Studio 14 2015 Win64" "-DCMAKE_INSTALL_PREFIX=C:\mongo-c-driver" "-DBSON_ROOT_DIR=C:\mongo-c-driver"
  msbuild.exe ALL_BUILD.vcxproj
  msbuild.exe INSTALL.vcxproj

All of the MongoDB C Driver's components will now be found in ``C:\mongo-c-driver``.

Native TLS Support on Windows (Secure Channel)
----------------------------------------------

	The MongoDB C Driver supports the Windows native TLS and crypto libraries.
	Using the native libraries there is no need to install OpenSSL. By
	default however, the driver will compile against OpenSSL if it
	detects it being available. If OpenSSL is not available, it will
	fallback on the native libraries.
      

	To compile against the Windows native TLS and crypto libraries, even when
	OpenSSL is available, configure the driver like so:
      

.. code-block:: none

  cmake -G "Visual Studio 14 2015 Win64" "-DENABLE_SSL=WINDOWS" "-DCMAKE_INSTALL_PREFIX=C:\mongo-c-driver" "-DBSON_ROOT_DIR=C:\mongo-c-driver"

