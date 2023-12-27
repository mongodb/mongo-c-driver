##############################################
Installing Prebuilt MongoDB C Driver Libraries
##############################################

.. highlight:: shell-session
.. default-role:: bash

.. Links:

.. _EPEL: https://docs.fedoraproject.org/en-US/epel/
.. _Homebrew: https://brew.sh/

The |libmongoc| and |libbson| libraries are often available in the package
management repositories of :ref:`common Linux distributions <installing.linux>` and
:ref:`macOS via Homebrew <installing.macos>`.

.. note::

  For :index:`Windows`, it is recommended to instead
  :doc:`build the libraries from source <from-source>`, for maximum
  compatibility with the local toolchain. Building from source can be automated
  by using a from-source library package management tool such as Conan_ or
  vcpkg_ (See: :ref:`installing.pkgman`).

.. caution::

  If you install and use prebuilt binaries from a third-party packager, it is
  possible that it lags behind the version of the libraries described in these
  documentation pages (|version|). Note the version that you install and keep it
  in mind when reading these pages.

  For the most up-to-date versions of the C driver libraries, prefer to instead
  :doc:`build from source <from-source>`.

.. seealso::

  For a listing and common reference on available packages, refer to
  :doc:`/ref/packages`.


.. index::
  package managers; Conan
  package managers; vcpkg
  pair: installation; package managers

.. _installing.pkgman:

Cross Platform Installs Using Library Package Managers
******************************************************

Various library package managers offer |libbson| and |libmongoc| as installable
packages, including Conan_ and vcpkg_. This section will detail how to install
using those tools.

.. _conan: https://conan.io/
.. _vcpkg: https://vcpkg.io/


.. index::
  ! pair: installation; vcpkg

Installing using vcpkg
======================

.. note::
  This page will not detail how to get started using vcpkg_. For that, refer to
  `Get started with vcpkg`__

  __ https://vcpkg.io/en/getting-started

.. tab-set::

  .. tab-item:: vcpkg Manifest Mode (Recommended)

    In `vcpkg manifest mode`__, add the desired libraries to your project's
    ``vcpkg.json`` manifest file:

    __ https://learn.microsoft.com/en-us/vcpkg/users/manifests

    .. code-block:: js
      :caption: ``vcpkg.json``
      :linenos:

      {
        // ...
        "dependencies": [
          // ...
          "mongo-c-driver"
        ]
      }

    When you build a CMake project with vcpkg integration and have a
    ``vcpkg.json`` manifest file, vcpkg will automatically install the project's
    dependencies before proceeding with the configuration phase, so no
    additional manual work is required.


  .. tab-item:: vcpkg Classic Mode

    In `vcpkg classic mode`__, |libbson| and |libmongoc| can be installed through the
    names ``libbson`` and ``mongo-c-driver``, respectively::

      $ vcpkg install mongo-c-driver

    __ https://learn.microsoft.com/en-us/vcpkg/users/classic-mode

    (Installing ``mongo-c-driver`` will transitively install |libbson| as well.)

    When the |libmongoc| and |libbson| packages are installed and vcpkg has been
    properly integrated into your build system, the desired libraries will be
    available for import.

With CMake, the standard config-file package will be available, as well as the
generated ``IMPORTED`` targets:

.. code-block:: cmake
  :caption: ``CMakeLists.txt``

  find_package(mongoc-1.0 CONFIG REQUIRED)
  target_link_libraries(my-application
      PRIVATE $<IF:$<TARGET_EXISTS:mongo::mongoc_shared>,mongo::mongoc_shared,mongo::mongoc_static>)

.. note::

  The large ``$<IF:$<TARGET_EXISTS...>:...>`` generator expression
  (:external:doc:`manual/cmake-generator-expressions.7`) can be used to switch
  the link type of |libmongoc| based on whichever form is available from the
  ``find_package()`` command. |libmongoc| supports building with both *dynamic*
  and *static* library types, but vcpkg will only install one of the two library
  types at a time.

Configuring a CMake project with vcpkg integration is a matter of setting the
CMake toolchain file at the initial configure command::

  $ cmake -S . -B _build -D CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

.. index::
  ! pair: Linux; installation

.. _installing.linux:

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

.. seealso::

  For a list of available packages and package options, see:
  :doc:`/ref/packages`.


.. index::
  !pair: installation; RHEL
  !pair: installation; Fedora
  !pair: installation; CentOS
  !pair: installation; Rocky Linux
  !pair: installation; AlmaLinux
  !pair: installation; Yum
  !pair: installation; DNF
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
  respective enterprise Linux distributions. However, the C driver libraries
  are not available in the default repositories, but can be obtained by enabling
  the EPEL_ repositories. This can be done by installing the `epel-release`
  package::

    # yum install epel-release

  `epel-release` must be installed before attempting to install the C driver
  libraries (i.e. one cannot install them both in a single `yum install`
  command).

To install |libbson| only, install the `libbson-devel` package::

  # yum install libbson-devel

To install the full C database driver (|libmongoc|), install
`mongo-c-driver-devel`::

  ## (This package will transitively install libbson-devel)
  # yum install mongo-c-driver-devel

To check which version is available, see https://packages.fedoraproject.org/pkgs/mongo-c-driver/mongo-c-driver-devel.

The development packages (ending in `-devel`) include files required to build applications using |libbson| and |libmongoc|.
To only install the libraries without development files, install the `libbson` or `mongo-c-driver-libs` packages.

.. index::
    !pair: installation; Debian
    !pair: installation; Ubuntu
    !pair: installation; APT
.. _debian:

Debian-based Systems
====================

In Debian-based Linux distributions, including Ubuntu and Ubuntu derivatives,
|libbson| and |libmongoc| are available in the distribution repositories via
APT, and can be installed as `libbson-dev` and `libmongoc-dev`, respectively::

  ## Update repository information, if necessary:
  # apt update

To install only |libbson|::

  # apt install libbson-dev

To install |libmongoc| (which will also install |libbson|)::

  # apt install libmongoc-dev

To check which version is available, run `apt-cache policy libmongoc-dev`.

The development packages (ending in `-dev`) include files required to build applications using |libbson| and |libmongoc|.
To only install the libraries without development files, install the `libbson-1.0-0` or `libmongoc-1.0-0` packages.

.. index::
  !pair: installation; macOS
  !pair: installation; Homebrew
  package managers; Homebrew
.. _installing.macos:

Installing on macOS with Homebrew
*********************************

If you are using a macOS system, the C driver libraries (including both
|libmongoc| and |libbson|) may be installed using the Homebrew_ package manager
[#macos_brew]_ with the following command::

  $ brew install mongo-c-driver

.. note::

  Homebrew does not provide separate packages for |libbson| and |libmongoc|.

.. [#macos_brew]

  The Homebrew_ package manager is not installed by default on macOS. For
  information on installing Homebrew, refer to
  `the Homebrew installation documentation page <https://docs.brew.sh/Installation>`_.
