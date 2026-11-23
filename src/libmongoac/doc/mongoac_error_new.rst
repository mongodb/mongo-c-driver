:man_page: mongoac_error_new

mongoac_error_new()
===================

Synopsis
--------

.. code-block:: c

   #include <mongoac/error.h>

   mongoac_error_t *mongoac_error_new(void);

Description
-----------

Returns a new :symbol:`mongoac_error_t` initialized with its default state.

Must be destroyed with :symbol:`mongoac_error_destroy()`.

.. seealso::

   | :symbol:`mongoac_error_clone`
   | :symbol:`mongoac_error_destroy`
