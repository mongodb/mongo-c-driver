:man_page: mongoac_read_preference_set_tag_sets_none

mongoac_read_preference_set_tag_sets_none()
============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_set_tag_sets_none(mongoac_read_preference_t *read_preference);

Description
-----------

Sets the "tag sets" field of the given :symbol:`mongoac_read_preference_t` to ``None``.

When ``read_preference`` is null, does nothing.

When the "mode" field is "primary", does nothing.

.. seealso::

   | :symbol:`mongoac_read_preference_get_tag_sets_len`
   | :symbol:`mongoac_read_preference_set_tag_sets_push`
