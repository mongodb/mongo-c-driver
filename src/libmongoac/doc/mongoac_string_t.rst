:man_page: mongoac_string_t

mongoac_string_t
================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   typedef struct mongoac_string_t {
       const char *ptr;
       uintptr_t len;
   } mongoac_string_t;

Description
-----------

Represents an owning, read-only string.

.. important::

  All strings handled by the mongoac library are valid UTF-8 strings.

``mongoac_string_t`` is an owning, read-only view to a string:

- When ``ptr`` is null, ``len`` is unspecified (but typically set to ``0``).
- When ``ptr`` is not null, the range ``[ptr, ptr + len)`` is valid and accessible.

All ``mongoac_string_t`` objects must be destroyed with :symbol:`mongoac_string_destroy()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_string_destroy
