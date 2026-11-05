:man_page: mongoac_read_preference_get_mode

mongoac_read_preference_get_mode()
==================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   mongoac_read_preference_mode_t mongoac_read_preference_get_mode(const mongoac_read_preference_t *read_preference);

Description
-----------

Returns the `"mode" <mongoac_read_preference_mode_t>`_ field of the given :symbol:`mongoac_read_preference_t`.

When ``read_preference`` is null, returns ``MONGOAC_READ_PREFERENCE_MODE_UNKNOWN``.

When the mode is an unrecognized value, returns ``MONGOAC_READ_PREFERENCE_MODE_UNKNOWN``.

.. seealso::

   | :symbol:`mongoac_read_preference_t`
