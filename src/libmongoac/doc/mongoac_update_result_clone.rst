:man_page: mongoac_update_result_clone

mongoac_update_result_clone()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/update_result.h>

   mongoac_update_result_t *mongoac_update_result_clone(const mongoac_update_result_t *result);

Description
-----------

Returns a copy of the given :symbol:`mongoac_update_result_t`.

When ``result`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_update_result_destroy()`.

.. seealso::

   | :symbol:`mongoac_update_result_destroy`
