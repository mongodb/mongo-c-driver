:man_page: mongoac_error_clone

mongoac_error_clone()
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   mongoac_error_t *mongoac_error_clone(const mongoac_error_t *error);

Description
-----------

Returns a new :symbol:`mongoac_error_t` with the same state as ``error``.

When ``error`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_error_destroy()`.

.. seealso::

   | :symbol:`mongoac_error_destroy`
   | :symbol:`mongoac_error_new`
