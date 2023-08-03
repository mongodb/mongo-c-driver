##############################################
Installing Prebuilt MongoDB C Driver Libraries
##############################################

.. highlight:: shell-session
.. default-role:: bash

.. Links:

.. _EPEL: https://docs.fedoraproject.org/en-US/epel/
.. _Homebrew: https://brew.sh/

The |libmongoc| and |libbson| libraries are often available in the package
management repositories of `common Linux distributions <linux_>`_ and
`macOS via Homebrew <macos_>`_.

.. note::

  For Windows, it is recommended to instead
  :doc:`build the libraries from source <from-source>`, for maximum
  compatibility with the local toolchain.

.. caution::

  If you install and use prebuilt binaries from a third-party packager, it is
  possible that it lags behind the version of the libraries described in these
  documentation pages (|version|). Note the version that you install and keep it
  in mind when reading these pages.

  For the most up-to-date versions of the C driver libraries, prefer to instead
  :doc:`build from source <from-source>`.


.. _linux:

Installing in Linux
*******************

The names and process of installing |libbson| and |libmongoc| varies between
distributions, but generally follows a similar pattern.

The following Linux distributions provide |libbson| and |libmongoc| packages:

- `Fedora <redhat_>`_ via `dnf`
- `RedHat Enterprise Linux (RHEL) 7 and Newer <redhat_>`_ and distribusions
  based on RHEL 7 or newer, including
  `CentOS, Rocky Linux, and AlmaLinux <redhat_>`_, via `yum`/`dnf` and EPEL_.
- `Debian <debian_>`_ and Debian-based distributions, including
  `Ubuntu <debian_>`_ and Ubuntu derivatives, via APT.


.. _redhat:

RedHat-based Systems
====================

In RedHat-based Linux distributions, including **Fedora**, **CentOS**,
**Rocky Linux**, and **AlmaLinux**, the C driver libraries can be installed with
Yum/DNF.

.. note::

  For Fedora and enterprise Linux of version 8 or greater, it is recommended to
  use the `dnf` command in place of any `yum` command.

  .. XXX: Once RHEL 7 support is dropped, all supported RedHat systems will use
    `dnf`, so these docs should be updated accordingly.

.. important:: **Except for Fedora:**

  The C driver libraries are only available in version 7 and newer of the
  resepective enterprise Linux distributions. However, the C driver libraries
  are not available in the default repositories, but can be obtained by enabling
  the EPEL_ repositories. This can be done by installing the `epel-release`
  package::

    # yum install epel-release

  `epel-release` must be installed before attempting to install the C driver
  libraries (i.e. one cannot install them both in a single `yum intsall`
  command).

To install |libbson| only, install the `libbson-devel` package::

  # yum install libbson-devel

To install the full C database driver (|libmongoc|), install
`mongo-c-driver-devel`::

  ## (This package will transitively install libbson-devel)
  # yum install mongo-c-driver-devel


.. _debian:

Debian-based Systems
====================

In Debian-based Linux distributions, including Ubuntu and Ubuntu derivatives,
|libbson| and |libmongoc| are available in the distribution repositories via
APT, and can be installed as `libbson-dev` and `libmongoc-dev`, resepectively::

  ## Update repository information, if necessary:
  # apt update

To install only |libbson|::

  # apt install libbson-dev

To install |libmongoc| (which will also install |libbson|)::

  # apt install libmongoc-dev


.. _macos:

Installing on macOS with Homebrew
*********************************

If you are using a macOS system, the C driver libraries (including both
|libmongoc| and |libbson|) may be installed using the Homebrew_ package manager [#macos_brew]_
with the following command::

  $ brew install mongo-c-driver

.. note::

  Homebrew does not provide separate packages for |libbson| and |libmongoc|.


.. todo
  Packages for Windows, via Conan, and via vcpkg


.. [#macos_brew]

  The Homebrew_ package manager is not installed by default on macOS. For
  information on installing Homebrew, refer to
  `the Homebrew installation documentation page <https://docs.brew.sh/Installation>`_.
