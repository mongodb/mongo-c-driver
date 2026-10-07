:man_page: mongoac_error_set

mongoac_error_set()
===================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   void mongoac_error_set(mongoac_error_t *error, int32_t category, int32_t code);

Description
-----------

Manually assigns an error category and error code to the given :symbol:`mongoac_error_t`.

If ``error`` is a null pointer, does nothing.

.. seealso::

   | :symbol:`mongoac_error_clear()`
   | :symbol:`mongoac_error_category()`
   | :symbol:`mongoac_error_code()`
