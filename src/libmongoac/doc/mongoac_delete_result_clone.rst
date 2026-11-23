:man_page: mongoac_delete_result_clone

mongoac_delete_result_clone()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/delete_result.h>

   mongoac_delete_result_t *mongoac_delete_result_clone(const mongoac_delete_result_t *result);

Description
-----------

Returns a copy of the given :symbol:`mongoac_delete_result_t`.

When ``result`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_delete_result_destroy()`.

.. seealso::

   | :symbol:`mongoac_delete_result_destroy`
