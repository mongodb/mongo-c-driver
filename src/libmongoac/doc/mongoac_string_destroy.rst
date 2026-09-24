:man_page: mongoac_string_destroy

mongoac_string_destroy()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   void mongoac_string_destroy(mongoac_string_t string);

Description
-----------

Destroys the given :symbol:`mongoac_string_t`.

When ``string.ptr`` is null, does nothing.

.. important::

  When not null, ``string.ptr`` and ``string.len`` MUST equal the original values that were returned by the mongoac
  library when the string was allocated.
