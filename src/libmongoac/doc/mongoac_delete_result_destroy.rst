:man_page: mongoac_delete_result_destroy

mongoac_delete_result_destroy()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/delete_result.h>

   void mongoac_delete_result_destroy(mongoac_delete_result_t *result);

Description
-----------

Destroys the given :symbol:`mongoac_delete_result_t`.

When ``result`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_delete_result_clone`
