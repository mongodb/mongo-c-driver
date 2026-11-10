:man_page: mongoac_read_preference_get_max_staleness_is_none

mongoac_read_preference_get_max_staleness_is_none()
===================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   bool mongoac_read_preference_get_max_staleness_is_none(const mongoac_read_preference_t *read_preference);

Description
-----------

Returns ``true`` when the "max staleness" field of the given :symbol:`mongoac_read_preference_t` is ``None``.

When ``read_preference`` is null, returns ``true``.

.. seealso::

   | :symbol:`mongoac_read_preference_set_max_staleness_from_secs`
   | :symbol:`mongoac_read_preference_set_max_staleness_none`
