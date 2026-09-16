:man_page: mongoac_version

mongoac_version
===============

Synopsis
--------

.. code-block:: c

   #include <mongoac/version.h>

   #define MONGOAC_VERSION            // e.g. "1.2.3-dev"
   #define MONGOAC_VERSION_MAJOR      // e.g. 1
   #define MONGOAC_VERSION_MINOR      // e.g. 2
   #define MONGOAC_VERSION_PATCH      // e.g. 3
   #define MONGOAC_VERSION_PRERELEASE // e.g. "dev" or ""

   #define MONGOAC_VERSION_HEX // e.g. 0x01020300

   // Return true when the library version is greater than or equal to the specified version.
   //
   // e.g. given mongoac library version 1.2.3:
   //
   //    MONGOAC_CHECK_VERSION(0, 1, 0) -> true
   //    MONGOAC_CHECK_VERSION(1, 0, 0) -> true
   //    MONGOAC_CHECK_VERSION(1, 2, 0) -> true
   //    MONGOAC_CHECK_VERSION(1, 2, 3) -> true
   //    MONGOAC_CHECK_VERSION(1, 2, 4) -> false
   //    MONGOAC_CHECK_VERSION(1, 3, 0) -> false
   //    MONGOAC_CHECK_VERSION(2, 0, 0) -> false
   #define MONGOAC_CHECK_VERSION(major, minor, patch)

   int32_t mongoac_version_major(void); // Runtime equivalent to MONGOAC_VERSION_MAJOR.
   int32_t mongoac_version_minor(void); // Runtime equivalent to MONGOAC_VERSION_MINOR.
   int32_t mongoac_version_patch(void); // Runtime equivalent to MONGOAC_VERSION_PATCH.

   int32_t mongoac_version_hex(void); // Runtime equivalent to MONGOAC_VERSION_HEX.

   // Runtime equivalent to MONGOAC_CHECK_VERSION.
   bool mongoac_check_version(int32_t major, int32_t minor, int32_t patch);

Description
-----------

Defines preprocessor macros and functions which describe the mongoac library version.

.. note::

   ``MONGOAC_CHECK_VERSION`` and ``mongoac_check_version()`` return ``true`` when the library version is *greater than or equal to* the required version.
   To ensure major version compatibility, use ``MONGOAC_VERSION_MAJOR`` or ``mongoac_version_major()``.
