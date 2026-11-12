:man_page: mongoac_read_preference_destroy

mongoac_read_preference_destroy()
=================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_destroy(mongoac_read_preference_t *read_preference);

Description
-----------

Destroys the given :symbol:`mongoac_read_preference_t`.

When ``read_preference`` is null, does nothing.

.. seealso::

   | :symbol:`mongoac_read_preference_clone`
   | :symbol:`mongoac_read_preference_new`
