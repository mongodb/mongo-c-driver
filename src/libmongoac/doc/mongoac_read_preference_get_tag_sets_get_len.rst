:man_page: mongoac_read_preference_get_tag_sets_get_len

mongoac_read_preference_get_tag_sets_get_len()
==============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   uintptr_t mongoac_read_preference_get_tag_sets_get_len(const mongoac_read_preference_t *read_preference,
                                                          uintptr_t index);

Description
-----------

Returns the number of tags in the tag set at ``index`` in the "tag sets" field of the given :symbol:`mongoac_read_preference_t`.

When the field is ``None`` or ``index`` is out of bounds, returns ``0``.

When ``read_preference`` is null, returns ``0``.

.. seealso::

   | :symbol:`mongoac_read_preference_get_tag_sets_get_get`
   | :symbol:`mongoac_read_preference_get_tag_sets_get_keys`
   | :symbol:`mongoac_read_preference_get_tag_sets_len`
   | :symbol:`mongoac_read_preference_set_tag_sets_push`
