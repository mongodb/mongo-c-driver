libmongoac - API
================

The MongoDB Async C Driver.

.. caution::

  The mongoac library is **experimental** and not ready for production use!

Introduction
------------

The MongoDB Async C Driver (mongoac) is an official MongoDB client library providing an asynchronous C API by wrapping the `MongoDB Rust Driver <https://www.mongodb.com/docs/drivers/rust/current/>`_.
It is an independent async alternative to `MongoDB C Driver <https://www.mongodb.com/docs/drivers/c/current/>`_ (mongoc).

Usage Requirements
------------------

To use a prebuilt mongoac library, the following requirements must be satisfied:

- **C Compiler: C99 or newer**
    - Required by all mongoac headers.
- **CMake: 3.23 or newer**
    - The recommended method to import the mongoac library via ``find_package(mongoac)`` and targets ``mongoac::shared`` or ``mongoac::static``.
- **pkgconf / pkg-config**
    - An alternative method to CMake via ``pkgconf`` or ``pkg-config`` (``mongoac0.pc`` and ``mongoac0-static.pc``).

Build Requirements
------------------

To build the mongoac library, it must be enabled by setting the CMake configuration option ``ENABLE_MONGOAC=ON``.

When enabled, building the mongoac library requires the following toolchain dependencies:

- **CMake: 3.25 or newer**
    - Required to support file sets, path manipulation, and other features used for Rust Toolchain integration.
- **Rust Toolchain: 1.88 or newer**
    - Required to build the mongoac library implementation (written in Rust).
    - The ``cargo`` executable is discovered using ``find_program()`` in CMake.
    - See ``src/libmongoac/Cargo.toml`` for further details.
- **C Compiler: GCC 8.1+, Clang 3.8+, AppleClang 13.1+ (Xcode 13.4.1+), or MSVC 19.16.27023+ (Visual Studio 2017 15.9+)**
    - Required to build C components used by certain Rust crates (e.g. AWS-LC, zstd).
    - Optional: required to validate headers when ``CMAKE_VERIFY_INTERFACE_HEADER_SETS=ON`` by building the CMake target ``mongoac_shared_verify_interface_header_sets`` or ``mongoac_static_verify_interface_header_sets``.
- **C++ Compiler: C++17 or newer**
    - Optional: only required to build ``test-libmongoac`` when ``ENABLE_TESTS=ON``.
- **patchelf (Linux only)**
    - Required to set the ``SONAME`` of the shared library; a warning is emitted when not found.
    - Optional: only on Linux (per CMake variable ``LINUX``) when building the shared library (``ENABLE_SHARED=ON``).

Supported Platforms
-------------------

mongoac is built and tested in CI on the following platform only. RHEL 8 x86_64 is the client baseline platform: it
provides the oldest glibc in the CI distro catalog (2.28), and its GCC 8.5 exercises the documented GCC 8.1 minimum.
It is also the baseline platform of the MongoDB Rust Driver.

.. list-table::
   :header-rows: 1

   * - Platform
     - C Compiler
     - Rust Toolchain
     - Status
   * - RHEL 8, x86_64
     - GCC 8.5
     - 1.88
     - Tested in CI

Building and testing mongoac on this platform additionally requires:

- **CMake: 3.25 or newer**
    - See `Build Requirements`_.
- **C++ Compiler: C++17 or newer**
    - Required to build ``test-libmongoac``.
- **patchelf (Linux only)**
    - Used to set the ``SONAME`` of the shared library (see `Build Requirements`_).

Additional (Untested) Platforms
-------------------------------

The following platforms are expected to work, but are not tested in CI:

- **Other glibc-based x86_64 Linux distributions**, such as RHEL 9, Ubuntu 22.04 or newer, and Debian 12 or newer.
- **Other compilers at the declared minimums**: GCC 8.1+, Clang 3.8+, AppleClang 13.1+ (Xcode 13.4.1+), or MSVC
  19.16.27023+ (Visual Studio 2017 15.9+).

  .. note::

     Meeting a compiler minimum does not make a platform supported: macOS and Windows remain unsupported for the
     reasons given below.

Unsupported Platforms
---------------------

The following platforms are not supported. mongoac does not attempt to handle them, and builds on them are untested:

- **macOS**
    - The Mach-O install name (``LC_ID_DYLIB``) of the shared library is not rewritten. The ``SONAME`` is set with
      ``patchelf`` only on Linux; no macOS equivalent (e.g. ``install_name_tool``) is applied.
    - The shared library is renamed and symlinked using ELF-style versioned names during installation
      (e.g. ``libmongoac0.dylib.0.1.0``), which does not follow the macOS ``.dylib`` naming convention.
    - The build emits ``The mongoac shared library has not yet been tested on macOS!`` when ``ENABLE_SHARED=ON``.
- **Windows**
    - The import library emitted by Cargo for the shared library (e.g. ``mongoac.dll.lib``) is not handled: only the
      ``.dll`` is copied and installed.
    - The install rules unconditionally create versioned symbolic links (``file (CREATE_LINK ... SYMBOLIC)``);
      symbolic links are not supported by default on Windows.
    - The build emits ``The mongoac shared library has not yet been tested on Windows!`` when ``ENABLE_SHARED=ON``.
- **Cross-compilation**
    - Cargo is invoked without ``--target`` and ``CARGO_BUILD_TARGET`` is not honored, so the library is always built
      for the host platform.
- **Non-x86_64 architectures** (e.g. ARM, POWER, s390x)
    - Untested in CI. Some Rust dependencies (e.g. ``aws-lc-sys`` and ``zstd-sys``) compile native C code whose
      requirements on other architectures have not been verified.

API Documentation
-----------------

.. toctree::
  :titlesonly:
  :maxdepth: 2

  api
