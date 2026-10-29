:man_page: mongoac_string_view_array_t

mongoac_string_view_array_t
===========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   typedef struct mongoac_string_view_array_t {
       const mongoac_string_view_t *ptr;
       uintptr_t len;
   } mongoac_string_view_array_t;

Description
-----------

Represents a non-owning, read-only array of :symbol:`mongoac_string_view_t`.

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

Lifetime
--------

The object from which a view is obtained MUST outlive any access to the pointed-to data.
