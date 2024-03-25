##############################
Package Installation Reference
##############################

.. _EPEL: https://docs.fedoraproject.org/en-US/epel/
.. _conan: https://conan.io/
.. _vcpkg: https://vcpkg.io/

|libbson| and |libmongoc| are available from several package management tools
on a variety of systems.

.. important::

  The third-party packages detailed here are not directly controlled via the
  |mongo-c-driver| maintainers, and the information found here may be incomplete
  or out-of-date.


Package Names and Availability
******************************

This table details the names and usage notes of such packages.

.. note::
  
  The development packages (ending in ``-dev`` or ``-devel``) include files required to build applications using |libbson| and |libmongoc|.

.. seealso::

  For a step-by-step tutorial on installing packages, refer to
  :doc:`/learn/get/installing`.


.. list-table::
  :name: Available Packages
  :header-rows: 1
  :align: left
  :widths: auto

  - - Packaging Tool
    - Platform(s)
    - |libbson| package(s)
    - |libmongoc| package(s)
    - Notes

  - - :index:`APT <!pair: APT; package names>` (``apt``/``apt-get``)
    - Debian-based Linux distributions (:index:`Debian`, :index:`Ubuntu`, Linux
      Mint, etc.)
    - ``libbson-1.0-0``, ``libbson-dev``,
      ``libbson-doc``
    - ``libmongoc-1.0-0``, ``libmongoc-dev``,
      ``libmongoc-doc``
    - .. empty cell

  - - :index:`Yum <!pair: Yum; package names>`/:index:`DNF <!pair: DNF; package names>`
    - RHEL-based systems (RHEL, :index:`Fedora`, :index:`CentOS`,
      :index:`Rocky Linux`, :index:`AlmaLinux`)
    - ``libbson``, ``libbson-devel``
    - ``mongo-c-driver-libs``, ``mongo-c-driver-devel``
    - *Except on Fedora* the EPEL_ repositories must be enabled (i.e. install
      the ``epel-release`` package first)

  - - :index:`APK <!pair: APK; package names>`
    - :index:`Alpine Linux <!pair: Alpine Linux; package names>`
    - ``libbson``, ``libbson-dev``, ``libbson-static``
    - ``mongo-c-driver``, ``mongo-c-driver-dev``, ``mongo-c-driver-static``
    - .. empty cell

  - - :index:`pacman <!pair: package names; pacman>`
    - :index:`Arch Linux <!pair: package names; Arch Linux>`
    - ``mongo-c-driver``
    - ``mongo-c-driver``
    - A single package provides both runtime and development support for both
      |libbson| and |libmongoc|

  - - :index:`Homebrew <!pair: Homebrew; package names>`
    - :index:`macOS <!pair: macOS; package names>`
    - ``mongo-c-driver``
    - ``mongo-c-driver``
    - .. empty

  - - :index:`Conan <!pair: Conan; package names>`
    - Cross-platform
    - ``mongo-c-driver``
    - ``mongo-c-driver``
    - See: :ref:`ref.conan.settings`

  - - :index:`vcpkg <!pair: vcpkg; package names>`
    - Cross-platform
    - ``libbson``
    - ``mongo-c-driver``
    - See: :ref:`ref.vcpkg.features`


.. index:: !Conan; Settings and Features
.. _ref.conan.settings:

Conan Settings and Features
***************************

The ``mongo-c-driver`` Conan_ recipe includes several build settings that
correspond to the configure-time build settings available when building the
|mongo-c-driver| project.

.. seealso::

  `The mongo-c-driver Conan recipe (includes libbson)`__

  __ https://github.com/conan-io/conan-center-index/tree/master/recipes/mongo-c-driver

.. list-table::
  :header-rows: 1
  :align: left

  - - Setting
    - Options
    - Default
    - Notes

  - - ``shared``
    - (Boolean)
    - ``False``
    - Build the shared library instead of the static library

  - - ``fPIC``
    - (Boolean)
    - ``True``
    - Compile code as position-independent

  - - ``srv``
    - (Boolean)
    - ``True``
    - Enables MongoDB SRV URI support

  - - ``with_ssl``
    - ``openssl``, ``libressl``, ``windows``, ``darwin``, ``False``
    - ``openssl`` [#oss-default]_
    - Select a TLS backend. Setting to "``False``" disables TLS support.

  - - ``with_sasl``
    - ``sspi``, ``cyrus``, ``False``
    - ``sspi`` on Windows, ``False`` elsewhere
    - Enable `SASL authentication`__ support

      __ https://en.wikipedia.org/wiki/Simple_Authentication_and_Security_Layer

  - - ``with_snappy``
    - (Boolean)
    - ``True``
    - Enable Snappy_ compression

      .. _snappy: https://google.github.io/snappy/

  - - ``with_zlib``
    - (Boolean)
    - ``True``
    - Enable Zlib__ compression

      __ https://www.zlib.net/

  - - ``with_zstd``
    - (Boolean)
    - ``True``
    - Enable Zstd_ compression

      .. _zstd: https://github.com/facebook/zstd

.. [#oss-default]

  Conan will use OpenSSL as the default TLS backend, even on platforms that ship
  with their own TLS implementation (e.g. Windows and macOS). This behavior
  differs from the upstream default-configured |libmongoc| or the vcpkg
  distribution of |mongo-c-driver|, which both default to use the TLS
  implementation preferred for the target platform.

.. index:: !vcpkg; Optional features
.. _ref.vcpkg.features:

vcpkg Optional Features
***********************

The ``mongo-c-driver`` package offered by vcpkg_ supports several optional
features.

.. seealso::

  - `The vcpkg libbson port`__
  - `The vcpkg mongo-c-driver port`__

  __ https://github.com/microsoft/vcpkg/tree/master/ports/libbson
  __ https://github.com/microsoft/vcpkg/tree/master/ports/mongo-c-driver

.. list-table::
  :header-rows: 1
  :align: left

  - - Feature
    - Notes

  - - ``icu``
    - Installs the ICU library, which is necessary for non-ASCII usernames and
      passwords in pre-1.25 |libmongoc|
  - - ``openssl``
    - Use OpenSSL for encryption, even on Windows and Apple platforms which
      provide a native TLS backend.

      If omitted, the default will be to use the preferred TLS implementation
      for the system.
  - - ``snappy``
    - Enable the Snappy_ compression backend
  - - ``zstd``
    - Enable the Zstd_ compression backend
