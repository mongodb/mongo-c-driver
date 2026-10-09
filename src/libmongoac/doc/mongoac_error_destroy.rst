:man_page: mongoac_error_destroy

mongoac_error_destroy()
=======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   void mongoac_error_destroy(mongoac_error_t *error);

Description
-----------

Destroys the given :symbol:`mongoac_error_t`.

When ``error`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_error_clone`
   | :symbol:`mongoac_error_new`
