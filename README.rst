==============
mongo-c-driver
==============

About
=====

mongo-c-driver is a client library written in C for MongoDB.

There are absolutely no guarantees of API/ABI stability at this point.
But generally, we won't break API/ABI unless we have good reason.

mongo-c-driver depends on `Libbson <https://github.com/mongodb/libbson>`_.
Libbson will automatically be built if you do not have it installed on your system.

If you are looking for the legacy C driver, it can be found in the
`legacy branch <https://github.com/mongodb/mongo-c-driver/tree/legacy>`_.

Documentation / Support / Feedback
==================================

The documentation is available at http://api.mongodb.org/c/current/.
For issues with, questions about, or feedback for libmongoc, please look into
our `support channels <http://www.mongodb.org/about/support>`_. Please
do not email any of the libmongoc developers directly with issues or
questions - you're more likely to get an answer on the `mongodb-user list`_
on Google Groups.

Bugs / Feature Requests
=======================

Think you’ve found a bug? Want to see a new feature in libmongoc? Please open a
case in our issue management tool, JIRA:

- `Create an account and login <https://jira.mongodb.org>`_.
- Navigate to `the CDRIVER project <https://jira.mongodb.org/browse/CDRIVER>`_.
- Click **Create Issue** - Please provide as much information as possible about the issue type and how to reproduce it.

Bug reports in JIRA for all driver projects (i.e. CDRIVER, CSHARP, JAVA) and the
Core Server (i.e. SERVER) project are **public**.

How To Ask For Help
-------------------

If you are having difficulty building the driver after reading the below instructions, please email
the `mongodb-user list`_ to ask for help. Please include in your email all of the following
information:

- The version of the driver you are trying to build (branch or tag).
    - Examples: master branch, 1.0.2 tag
- Host OS, version, and architecture.
    - Examples: Windows 8 64-bit x86, Ubuntu 12.04 32-bit x86, OS X Mavericks
- C Compiler and version.
    - Examples: GCC 4.8.2, MSVC 2013 Express, clang 3.4, XCode 5
- The output of ``./autogen.sh`` or ``./configure`` (depending on whether you are building from a
  repository checkout or from a tarball). The output starting from "libbson was configured with
  the following options" is sufficient.
- The text of the error you encountered.

Failure to include the relevant information will result in additional round-trip
communications to ascertain the necessary details, delaying a useful response.
Here is a made-up example of a help request that provides the relevant
information:

  Hello, I'm trying to build the C driver with SSL, from mongo-c-driver-1.1.1.tar.gz. I'm on Ubuntu
  14.04, 64-bit Intel, with gcc 4.8.2. I run configure like::

    $ ./configure --enable-sasl=yes
    checking for gcc... gcc
    checking whether the C compiler works... yes

    ... SNIPPED OUTPUT, but when you ask for help, include full output without any omissions ...

    checking for pkg-config... no
    checking for SASL... no
    checking for sasl_client_init in -lsasl2... no
    checking for sasl_client_init in -lsasl... no
    configure: error: You must install the Cyrus SASL libraries and development headers to enable SASL support.

  Can you tell me what I need to install? Thanks!

.. _mongodb-user list: http://groups.google.com/group/mongodb-user

Security Vulnerabilities
------------------------

If you’ve identified a security vulnerability in a driver or any other
MongoDB project, please report it according to the `instructions here
<http://docs.mongodb.org/manual/tutorial/create-a-vulnerability-report>`_.


Building from Release Tarball
=============================

Unless you intend on contributing to the mongo-c-driver, you will want to build
from a release tarball.

The most current release is 1.1.1 which you can download here.
`mongo-c-driver-1.1.1.tar.gz <https://github.com/mongodb/mongo-c-driver/releases/download/1.1.1/mongo-c-driver-1.1.1.tar.gz>`_.

To build on UNIX-like systems, do the following::

  $ tar xzf mongo-c-driver-1.1.1.tar.gz
  $ cd mongo-c-driver-1.1.1
  $ ./configure
  $ make
  $ sudo make install

To see all of the options available to you during configuration, run::

  $ ./configure --help

To build on Windows Vista or newer with Visual Studio 2010, do the following::

  cd mongo-c-driver-1.1.1
  cd src\libbson
  cmake -DCMAKE_INSTALL_PREFIX=C:\usr -G "Visual Studio 10 Win64" .
  msbuild.exe ALL_BUILD.vcxproj
  msbuild.exe INSTALL.vcxproj
  cd ..\..
  cmake -DCMAKE_INSTALL_PREFIX=C:\usr -DBSON_ROOT_DIR=C:\usr -G "Visual Studio 10 Win64" .
  msbuild.exe ALL_BUILD.vcxproj
  msbuild.exe INSTALL.vcxproj

Building From Git
=================

mongo-c-driver contains a copy of libbson in the case that your system does
not already have libbson installed. The configure script will detect if
libbson is not installed and install it too.

Dependencies
------------

Fedora::

  $ sudo yum install git gcc automake autoconf libtool

Debian::

  $ sudo apt-get install git gcc automake autoconf libtool

FreeBSD::

  $ su -c 'pkg install git gcc automake autoconf libtool'


Fetch Sources and Build
-----------------------

You can use the following to checkout and build mongo-c-driver::

  git clone https://github.com/mongodb/mongo-c-driver.git
  cd mongo-c-driver
  ./autogen.sh
  make
  sudo make install

In standard automake fasion, ./autogen.sh only needs to be run once.
You can use ./configure directly going forward.
Also, see ./configure --help for all configure options.


Building on Windows
===================

Currently, the cmake build system for mongo-c-driver does not build the libbson
package as well. This needs to be done manually with cmake.

SSL is supported through the use of OpenSSL. SASL is not currently supported
but is planned. To enable OpenSSL support, install the appropriate OpenSSL for
Windows from `here <http://slproweb.com/products/Win32OpenSSL.html>`_. The
instructions below assume 64-bit builds, so you would want to get the version
for "Win64 OpenSSL 1.0.1f" which includes libraries and headers.

If you are building from git, and not a release tarball, you also need to
initialize the git submodule for libbson::

  git submodule init
  git submodule update

Then proceed to build and install libbson using cmake and Visual Studio's
command line tool, msbuild.exe. You can of course open these project files
from Visual Studio as well::

  cd src\libbson
  cmake -DCMAKE_INSTALL_PREFIX=C:\usr -G "Visual Studio 10 Win64" .
  msbuild.exe ALL_BUILD.vcxproj
  msbuild.exe INSTALL.vcxproj
  cd ..\..
  cmake -DCMAKE_INSTALL_PREFIX=C:\usr -DBSON_ROOT_DIR=C:\usr -G "Visual Studio 10 Win64" .
  msbuild.exe ALL_BUILD.vcxproj
  msbuild.exe INSTALL.vcxproj


Generating the Docs
===================

To generate the documentation you must install the :code:`yelp-tools` package.
On Linux this package can be found in the package manager for your distribution,
on OSX we recommend using `TingPing's homebrew-gnome tap <https://github.com/TingPing/homebrew-gnome>`_.

Then use the following :code:`./configure` options:

* :code:`--enable-html-docs` - builds the HTML documentation
* :code:`--enable-man-pages` - builds and installs the man-pages.
