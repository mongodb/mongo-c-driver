:man_page: mongoac_bson_view_array_t

mongoac_bson_view_array_t
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/bson.h>

   typedef struct mongoac_bson_view_array_t {
       const mongoac_bson_view_t *ptr;
       uintptr_t len;
   } mongoac_bson_view_array_t;

Description
-----------

Represents a non-owning, read-only array of :symbol:`mongoac_bson_view_t`:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

Lifetime
--------

The object from which a view is obtained MUST outlive any access to the pointed-to data.
