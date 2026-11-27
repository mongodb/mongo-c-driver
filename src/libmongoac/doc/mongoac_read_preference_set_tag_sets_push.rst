:man_page: mongoac_read_preference_set_tag_sets_push

mongoac_read_preference_set_tag_sets_push()
===========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/read_preference.h>

   void mongoac_read_preference_set_tag_sets_push(mongoac_read_preference_t *read_preference,
                                                  mongoac_string_view_array_t keys,
                                                  mongoac_string_view_array_t values,
                                                  mongoac_error_t *error);

Description
-----------

Appends the given tag set ``keys`` and ``values`` to the "tag sets" field of the given :symbol:`mongoac_read_preference_t`.

``keys`` and ``values`` must have the same length.
A null array is equivalent to an empty array.

Arguments are validated regardless of the current value of the "mode" field.
When the "mode" field is "primary", inputs are validated but otherwise ignored (no tag set is appended).
For all other modes, a new tag set is appended.
When ``keys`` and ``values`` are empty, an empty tag set is appended.

Key-value pairs are inserted in the order of elements in the arrays.
When a duplicate key exists in ``keys``, the last corresponding element in ``values`` is used.

Undefined Behavior
------------------

When the "tag sets" field requires a new capacity that exceeds ``isize::MAX`` bytes, the behavior is undefined.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``read_preference`` is null.
- Invalid Argument (mongoac): an element in ``keys`` or ``values`` is null or invalid UTF-8.
- Invalid Argument (mongoac): ``keys`` and ``values`` have different lengths.

.. seealso::

   | :symbol:`mongoac_error_t`
   | :symbol:`mongoac_read_preference_get_tag_sets_get_get()`
   | :symbol:`mongoac_read_preference_set_tag_sets_none()`
   | :symbol:`mongoac_string_view_array_t`
