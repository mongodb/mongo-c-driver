:man_page: mongoac_string_view_array_destroy

mongoac_string_view_array_destroy()
===================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/string.h>

   void mongoac_string_view_array_destroy(mongoac_string_view_array_t array);

Description
-----------

Destroys the given :symbol:`mongoac_string_view_array_t`.

.. warning::

  The function from which the array was obtained MUST explicitly require that the array must be deallocated with :symbol:`mongoac_string_view_array_destroy()`.
  Otherwise, it is undefined behavior.

.. important::

  When not null, ``array.ptr`` and ``array.len`` MUST equal the original values that were returned by the mongoac library when the array was allocated.

.. seealso::

   | :symbol:`mongoac_read_preference_get_tag_sets_get_keys`
   | :symbol:`mongoac_string_view_array_t`
