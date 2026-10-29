:man_page: mongoac_string_array_t

mongoac_string_array_t
======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   typedef struct mongoac_string_array_t {
       const mongoac_string_t *ptr;
       uintptr_t len;
   } mongoac_string_array_t;

Description
-----------

Represents an owning, read-only array of :symbol:`mongoac_string_t`:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

All ``mongoac_string_array_t`` objects (and their elements) must be destroyed with :symbol:`mongoac_string_array_destroy()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_string_array_destroy
