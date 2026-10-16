:man_page: mongoac_bson_view_t

mongoac_bson_view_t
===================

Synopsis
--------

.. code-block:: c

   #include <mongoac/bson.h>

   typedef struct mongoac_bson_view_t {
       const uint8_t *ptr;
       uintptr_t len;
   } mongoac_bson_view_t;

Description
-----------

Represents a non-owning, read-only view to a BSON document:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

The object from which a view is obtained MUST outlive any access to the pointed-to data.
