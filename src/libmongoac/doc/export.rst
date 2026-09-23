:man_page: mongoac_export

Export Macros
=============

Synopsis
--------

.. code-block:: c

   #include <mongoac/export.h>

   #ifdef MONGOAC_STATIC
   #define MONGOAC_ABI_EXPORT
   #else
   #define MONGOAC_ABI_EXPORT /* ... (see below) ... */
   #endif

Description
-----------

Control symbol visibility in the public API.

When linking against the shared library, ``MONGOAC_ABI_EXPORT`` expands to platform-specific import directives. When ``MONGOAC_STATIC`` is defined, it expands to nothing.
