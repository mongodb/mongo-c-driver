:man_page: mongoac_read_preference_set_mode

mongoac_read_preference_set_mode()
==================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_set_mode(mongoac_read_preference_t *read_preference,
                                              mongoac_read_preference_mode_t v);

Description
-----------

Sets the read mode of the given :symbol:`mongoac_read_preference_t` to ``v``.

Valid values for ``v`` are listed in :symbol:`mongoac_read_preference_t`.

When ``read_preference`` is null, does nothing.

When ``v`` is unknown, does nothing.

.. seealso::

   | :symbol:`mongoac_read_preference_get_mode`
