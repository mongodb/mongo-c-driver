:man_page: mongoac_update_result_destroy

mongoac_update_result_destroy()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/update_result.h>

   void mongoac_update_result_destroy(mongoac_update_result_t *result);

Description
-----------

Destroys the given :symbol:`mongoac_update_result_t`.

When ``result`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_update_result_clone`
