:man_page: mongoac_error_clear

mongoac_error_clear()
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   void mongoac_error_clear(mongoac_error_t *error);

Description
-----------

Set the state of the given :symbol:`mongoac_error_t` to its default state.

When ``error`` is null, does nothing.

Equivalent to calling :symbol:`mongoac_error_set()` with ``MONGOAC_ERROR_CATEGORY_NONE`` and ``MONGOAC_ERROR_CODE_OK``.

.. seealso::

   | :symbol:`mongoac_error_set()`
   | :symbol:`mongoac_error_category()`
   | :symbol:`mongoac_error_code()`
