libmongoac - API
================

The MongoDB Async C Driver.

.. caution::

  The mongoac library is **experimental** and not ready for production use!

Introduction
------------

The MongoDB Async C Driver (mongoac) is an official MongoDB client library providing an asynchronous C API by wrapping the `MongoDB Rust Driver <https://www.mongodb.com/docs/drivers/rust/current/>`_.
It is an independent async alternative to the synchronous `MongoDB C Driver <https://www.mongodb.com/docs/drivers/c/current/>`_ (mongoc).

Usage Requirements
------------------

To use a prebuilt mongoac library, the following requirements must be satisfied:

- **C Compiler: C99 or newer**
    - Required by all mongoac headers.
- **CMake: 3.23 or newer**
    - The recommended method to import the mongoac library via ``find_package(mongoac)`` and targets ``mongoac::shared`` or ``mongoac::static``.
- **pkgconf / pkg-config**
    - An alternative to CMake via ``pkg-config`` (``libmongoac0.pc`` and ``libmongoac0-static.pc``).

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

API Documentation
-----------------

.. toctree::
  :titlesonly:
  :maxdepth: 2

  api
