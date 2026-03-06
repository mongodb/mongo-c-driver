:man_page: bson_append_array_begin

bson_append_array_begin()
=========================

.. warning::
   .. deprecated:: 2.3.0

      Use :symbol:`bson_append_array_builder_begin <bson_array_builder_t>` to safely generate array keys ("0", "1", "2", ...) or :symbol:`bson_append_array_unsafe_begin` to manually provide keys.

Synopsis
--------

.. code-block:: c

  #define BSON_APPEND_ARRAY_BEGIN(b, key, child) \
     bson_append_array_begin (b, key, (int) strlen (key), child)

  bool
  bson_append_array_begin (bson_t *bson,
                           const char *key,
                           int key_length,
                           bson_t *child);


Description
-----------

This function is a deprecated alias of :symbol:`bson_append_array_unsafe_begin`.

Consider using :symbol:`bson_array_builder_t` to append an array without needing to generate array element keys.

.. seealso::

  | :symbol:`bson_array_builder_t`
  | :symbol:`bson_append_array_unsafe_begin`

