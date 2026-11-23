:man_page: mongoac_read_preference_new

mongoac_read_preference_new()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   mongoac_read_preference_t *mongoac_read_preference_new(void);

Description
-----------

Returns a new :symbol:`mongoac_read_preference_t` in its default state ("primary").

Must be destroyed with :symbol:`mongoac_read_preference_destroy()`.

.. seealso::

   | :symbol:`mongoac_read_preference_clone`
   | :symbol:`mongoac_read_preference_destroy`
