:man_page: mongoac_read_preference_set_max_staleness_none

mongoac_read_preference_set_max_staleness_none()
================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_set_max_staleness_none(mongoac_read_preference_t *read_preference);

Description
-----------

Sets the "max staleness" field of the given :symbol:`mongoac_read_preference_t` to ``None``.

When ``read_preference`` is null, does nothing.

When the read mode is "primary", does nothing.
