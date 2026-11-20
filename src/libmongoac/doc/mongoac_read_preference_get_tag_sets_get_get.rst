:man_page: mongoac_read_preference_get_tag_sets_get_get

mongoac_read_preference_get_tag_sets_get_get()
==============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   mongoac_string_view_t mongoac_read_preference_get_tag_sets_get_get(const mongoac_read_preference_t *read_preference,
                                                                      uintptr_t index,
                                                                      mongoac_string_view_t key);

Description
-----------

Returns a :symbol:`mongoac_string_view_t` to the value corresponding to ``key`` in the tag set at ``index`` in the "tag sets" field of the given :symbol:`mongoac_read_preference_t`.

When ``read_preference`` is null, returns a null :symbol:`mongoac_string_view_t`.

When ``key`` is null or invalid UTF-8, returns a null :symbol:`mongoac_string_view_t`.

When the field is ``None``, ``index`` is out of bounds, or ``key`` is missing, returns a null :symbol:`mongoac_string_view_t`.

Lifetime
--------

The view may be invalidated when the associated :symbol:`mongoac_read_preference_t` is modified or destroyed.

.. seealso::

   | :symbol:`mongoac_read_preference_get_tag_sets_get_keys`
   | :symbol:`mongoac_read_preference_get_tag_sets_get_len`
   | :symbol:`mongoac_read_preference_get_tag_sets_len`
   | :symbol:`mongoac_read_preference_set_tag_sets_push`
   | :symbol:`mongoac_string_view_t`
