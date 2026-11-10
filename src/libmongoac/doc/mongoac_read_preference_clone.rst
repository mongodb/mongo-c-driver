:man_page: mongoac_read_preference_clone

mongoac_read_preference_clone()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   mongoac_read_preference_t *mongoac_read_preference_clone(const mongoac_read_preference_t *read_preference);

Description
-----------

Returns a new :symbol:`mongoac_read_preference_t` with the same state as ``read_preference``.

When ``read_preference`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_read_preference_destroy()`.

.. seealso::

   | :symbol:`mongoac_read_preference_destroy`
   | :symbol:`mongoac_read_preference_new`
