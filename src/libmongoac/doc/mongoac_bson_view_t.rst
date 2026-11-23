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

Represents a non-owning, read-only BSON document.

.. important::

  All documents returned by the mongoac library are valid BSON.

  Documents given to the mongoac library MUST be valid BSON.
  Input BSON arguments are `minimally validated <https://docs.rs/bson/latest/bson/struct.RawDocument.html#method.from_bytes>`_.
  Invalid BSON may produce unexpected runtime behavior.

``mongoac_bson_view_t`` is a non-owning, read-only view to a BSON document:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

Lifetime
--------

The object from which a view is obtained MUST outlive any access to the pointed-to data.
