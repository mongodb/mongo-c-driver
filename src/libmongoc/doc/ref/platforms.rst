#################################
|mongo-c-driver| Platform Support
#################################

This page documents information about the target platforms and toolchains that
are supported by the |mongo-c-driver| libraries.

Operating Systems
*****************

The following operating systems are continually tested with |mongo-c-driver|:

.. list-table::
  :header-rows: 1
  :align: left

  - - Operating System
    - Notes

  - - Debian
    - Versions **9.2**, **10.0**, and **11.0**
  - - RHEL
    - Versions **7.0**, **7.1**, **8.1**, **8.2**, and **8.3**. RHEL derivatives
      (e.g. CentOS, Rocky Linux, AlmaLinux) of the same release version are
      supported. Fedora is also supported, but not continually tested.
  - - Ubuntu
    - Versions **16.04**, **18.04**, and **20.04**. Subsequent minor releases
      are also supported. Ubuntu **22.04** and newer is not yet tested. Ubuntu
      derivatives based on supported Ubuntu versions are also supported.
  - - Arch Linux
    -
  - - macOS
    - Version **11.0**
  - - Windows Server 2008 and Windows Server 2016
    - Windows variants of the same generation are supported


Compilers
*********

The following compilers are continually tested for |mongo-c-driver|:

.. list-table::
  :header-rows: 1
  :align: left

  - - Compiler
    - Notes
  - - Clang
    - Versions **3.7**, **3.8**, and **6.0**. Newer versions
      are also supported, as well as the corresponding Apple Clang releases.
  - - GCC
    - Versions **4.8**, **5.4**, **6.3**, **7.5**, **8.2**, **8.3**,
      **9.4**, and **10.2**. The MinGW-w64 GCC is also tested and supported.
  - - Microsoft Visual C++ (MSVC)
    - Tested with MSVC **12.x** (Visual Studio **2013**), **14.x** (Visual
      Studio **2015**), and **15.x** (Visual Studio **2017**). Newer MSVC
      versions are supported but not yet tested.


Architectures
*************

The following CPU architectures are continually tested for |mongo-c-driver|:

.. list-table::
  :align: left
  :header-rows: 1

  - - Architecture
    - Notes
  - - x86 (32-bit)
    - Only tested on Windows
  - - x86_64 (64-bit x86)
    - Tested on Linux, macOS, and Windows
  - - ARM / aarch64
    - Tested on macOS and Linux
  - - Power8 (ppc64le)
    - Only tested on Linux
  - - zSeries (s390x)
    - Only tested on Linux


Others
******

Other platforms and toolchains are not tested, but similar versions of the above
platforms *should work*. If you encounter a platform or toolchain that you
expect to work and find that it does not, please open an issue describing the
problem, and/or open a `GitHub Pull Request`__ to fix it.

__ https://github.com/mongodb/mongo-c-driver/pulls

Simple pull requests to fix unsupported platforms are welcome, but will be
considered on a case-by-case basis. The acceptance of a pull request to fix the
libraries on an unsupported platform does not imply full support of that
platform.
