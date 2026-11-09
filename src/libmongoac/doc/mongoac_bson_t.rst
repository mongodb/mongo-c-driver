:man_page: mongoac_bson_t

mongoac_bson_t
==============

Synopsis
--------

.. code-block:: c

   #include <mongoac/bson.h>

   typedef struct mongoac_bson_t {
       const uint8_t *ptr;
       uintptr_t len;
   } mongoac_bson_t;

Description
-----------

Represents an owning, read-only BSON document.

.. important::

  All documents returned by the mongoac library are valid BSON.

``mongoac_bson_t`` is an owning, read-only view to a BSON document:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

All ``mongoac_bson_t`` objects must be destroyed with :symbol:`mongoac_bson_destroy()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_bson_destroy
