:man_page: mongoac_version

mongoac_version
===============

Synopsis
--------

.. code-block:: c

   #define MONGOAC_VERSION            // e.g. "1.2.3-dev"
   #define MONGOAC_VERSION_MAJOR      // e.g. 1
   #define MONGOAC_VERSION_MINOR      // e.g. 2
   #define MONGOAC_VERSION_PATCH      // e.g. 3
   #define MONGOAC_VERSION_PRERELEASE // e.g. "dev" or ""

   #define MONGOAC_VERSION_HEX // e.g. 0x01020300

   #define MONGOAC_VERSION_CHECK(major, minor, patch)

Description
-----------

Defines preprocessor macros describing the mongoac library version.
