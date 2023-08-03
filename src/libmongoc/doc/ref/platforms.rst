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

    * * Operating System
      * Notes

    * * Debian
      * Versions **9.2**, **10.0**, and **11.0**
    * * RHEL & RHEL Derivatives
      * Versions **7.0**, **7.1**, **8.1**, **8.2**, and **8.3**
    * * Ubuntu & Ubuntu Derivatives
      * Versions **16.04**, **18.04**, and **20.04**
    * * ArchLinux
      *
    * * macOS
      * Versions **10.14** and **11.0**
    * * Windows Server 2008 and Windows 2016
      * Windows variants of the same generation are supported


Compilers
*********

The following compilers are continually tested for |mongo-c-driver|:

.. list-table::
    :header-rows: 1

    - - Compiler
      - Notes
    - - Clang
      - Versions **3.4**, **3.5**, **3.7**, **3.8**, and **6.0**. Newer versions
        are also supported, as well as the corresponding Apple Clang releases.
    - - GCC
      - Versions **4.8**, **4.9**, **5.4**, **6.3**, **7.5**, **8.2**, **8.3**,
        **9.4**, and **10.2**. The MinGW-w64 GCC is also tested and supported.
    - - Visual C++
      - Tested with Visual Studio **2013**, **2015**, and **2017**.


Architectures
*************

The following CPU architectures are continually tested for |mongo-c-driver|:

.. list-table::
    :header-rows: 1

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
