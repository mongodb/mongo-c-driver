:man_page: mongoac_error_code

mongoac_error_code()
====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   int32_t mongoac_error_code(const mongoac_error_t *error);

Description
-----------

Returns the error code for the given :symbol:`mongoac_error_t`.

When ``error`` is null, returns ``MONGOAC_ERROR_CODE_OK``.

.. seealso::

   | :symbol:`mongoac_error_category`
