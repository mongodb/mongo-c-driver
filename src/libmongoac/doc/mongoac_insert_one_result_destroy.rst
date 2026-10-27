:man_page: mongoac_insert_one_result_destroy

mongoac_insert_one_result_destroy()
===================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_one_result.h>

   void mongoac_insert_one_result_destroy(mongoac_insert_one_result_t *result);

Description
-----------

Destroys the given :symbol:`mongoac_insert_one_result_t`.

When ``result`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_insert_one_result_clone`
