:man_page: mongoac_insert_one_result_clone

mongoac_insert_one_result_clone()
=================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_one_result.h>

   mongoac_insert_one_result_t *mongoac_insert_one_result_clone(const mongoac_insert_one_result_t *result);

Description
-----------

Returns a copy of the given :symbol:`mongoac_insert_one_result_t`.

When ``result`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_insert_one_result_destroy()`.

.. seealso::

   | :symbol:`mongoac_insert_one_result_destroy`
