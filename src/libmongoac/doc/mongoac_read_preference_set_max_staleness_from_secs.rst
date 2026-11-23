:man_page: mongoac_read_preference_set_max_staleness_from_secs

mongoac_read_preference_set_max_staleness_from_secs()
=====================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_set_max_staleness_from_secs(mongoac_read_preference_t *read_preference,
                                                            uint64_t v);

Description
-----------

Sets the "max staleness" field of the given :symbol:`mongoac_read_preference_t` to ``v`` (in seconds).

When ``read_preference`` is null, does nothing.

When the mode is "primary", does nothing.
