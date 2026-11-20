:man_page: mongoac_read_preference_get_tag_sets_get_keys

mongoac_read_preference_get_tag_sets_get_keys()
===============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   mongoac_string_view_array_t mongoac_read_preference_get_tag_sets_get_keys(const mongoac_read_preference_t *read_preference,
                                                                             uintptr_t index);

Description
-----------

Returns an array of :symbol:`mongoac_string_view_t` representing the keys in the tag set at ``index`` in the "tag sets" field of the given :symbol:`mongoac_read_preference_t`.

When the field is ``None`` or ``index`` is out of range, returns a null :symbol:`mongoac_string_view_array_t`.

When ``read_preference`` is null, returns a null :symbol:`mongoac_string_view_array_t`.

When the tag set exists but is empty, returns a non-null array with length ``0``.

Must be destroyed with :symbol:`mongoac_string_view_array_destroy()`.

.. note::

   The order of keys in the tag set is arbitrary.

.. note::

   The length of the array is equal to :symbol:`mongoac_read_preference_get_tag_sets_get_len()`.

Lifetime
--------

The views may be invalidated when the associated :symbol:`mongoac_read_preference_t` is modified or destroyed.

.. seealso::

   | :symbol:`mongoac_read_preference_get_tag_sets_get_get()`
   | :symbol:`mongoac_read_preference_get_tag_sets_get_len()`
   | :symbol:`mongoac_read_preference_get_tag_sets_len()`
   | :symbol:`mongoac_read_preference_set_tag_sets_push()`
   | :symbol:`mongoac_string_view_array_t`
