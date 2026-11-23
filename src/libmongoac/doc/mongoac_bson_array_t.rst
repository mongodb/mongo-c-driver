:man_page: mongoac_bson_array_t

mongoac_bson_array_t
====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/bson.h>

   typedef struct mongoac_bson_array_t {
       const mongoac_bson_t *ptr;
       uintptr_t len;
   } mongoac_bson_array_t;

Description
-----------

Represents an owning, read-only array of :symbol:`mongoac_bson_t`:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

All ``mongoac_bson_array_t`` objects (and their elements) must be destroyed with :symbol:`mongoac_bson_array_destroy()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_bson_array_destroy
