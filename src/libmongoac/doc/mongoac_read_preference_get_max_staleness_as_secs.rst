:man_page: mongoac_read_preference_get_max_staleness_as_secs

mongoac_read_preference_get_max_staleness_as_secs()
===================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   uint64_t mongoac_read_preference_get_max_staleness_as_secs(const mongoac_read_preference_t *read_preference);

Description
-----------

Returns the value (in seconds) of the "max staleness" field of the given :symbol:`mongoac_read_preference_t`.

When ``read_preference`` is null, returns ``0``.

When the field is ``None``, returns ``0``.

.. seealso::

   | :symbol:`mongoac_read_preference_set_max_staleness_from_secs`
   | :symbol:`mongoac_read_preference_set_max_staleness_none`
