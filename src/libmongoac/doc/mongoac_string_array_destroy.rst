:man_page: mongoac_string_array_destroy

mongoac_string_array_destroy()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   void mongoac_string_array_destroy(mongoac_string_array_t array);

Description
-----------

Destroys the given :symbol:`mongoac_string_array_t` and its elements.

When ``array.ptr`` is null, does nothing.

.. important::

  When not null, ``array.ptr`` and ``array.len`` MUST equal the original values that were returned by the mongoac library when the array was allocated.
