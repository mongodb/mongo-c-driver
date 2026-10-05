:man_page: mongoac_error_category

mongoac_error_category()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   int32_t mongoac_error_category(const mongoac_error_t *error);

Description
-----------

Returns the error category for the given :symbol:`mongoac_error_t`.

When ``error`` is null, returns ``MONGOAC_ERROR_CATEGORY_NONE``.

.. seealso::

   | :symbol:`mongoac_error_code`
