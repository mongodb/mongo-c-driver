:man_page: mongoac_string_view_t

mongoac_string_view_t
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   typedef struct mongoac_string_view_t {
       const char *ptr;
       uintptr_t len;
   } mongoac_string_view_t;

Description
-----------

Represents a non-owning, read-only string.

.. important::

  All strings handled by the mongoac library are valid UTF-8 strings.

``mongoac_string_view_t`` is a non-owning, read-only view to a string:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

The object from which a view is obtained MUST outlive any access to the pointed-to data.
