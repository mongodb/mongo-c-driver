:man_page: mongoac_error_message

mongoac_error_message()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   mongoac_string_t mongoac_error_message(const mongoac_error_t *error);

Description
-----------

Returns the error message for the given :symbol:`mongoac_error_t` when available.

When ``error`` is null or an error message is not available, returns a null string.

.. seealso::

   | :symbol:`mongoac_error_code`
   | :symbol:`mongoac_error_category`
