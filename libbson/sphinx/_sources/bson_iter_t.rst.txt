:man_page: bson_iter_t

bson_iter_t
===========

BSON Document Iterator

Synopsis
--------

.. code-block:: c

  #include <bson.h>

  typedef struct {
     /*< private >*/
  } bson_iter_t;

Description
-----------

:symbol:`bson_iter_t <bson_iter_t>` is a structure used to iterate through the elements of a :symbol:`bson_t <bson_t>`. It is meant to be used on the stack and can be discarded at any time as it contains no external allocation. The contents of the structure should be considered private and may change between releases, however the structure size will not change.

The :symbol:`bson_t <bson_t>` *MUST* be valid for the lifetime of the iter and it is an error to modify the :symbol:`bson_t <bson_t>` while using the iter.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_iter_array
    bson_iter_as_bool
    bson_iter_as_int64
    bson_iter_binary
    bson_iter_bool
    bson_iter_code
    bson_iter_codewscope
    bson_iter_date_time
    bson_iter_dbpointer
    bson_iter_decimal128
    bson_iter_document
    bson_iter_double
    bson_iter_dup_utf8
    bson_iter_find
    bson_iter_find_case
    bson_iter_find_descendant
    bson_iter_init
    bson_iter_init_find
    bson_iter_init_find_case
    bson_iter_int32
    bson_iter_int64
    bson_iter_key
    bson_iter_next
    bson_iter_oid
    bson_iter_overwrite_bool
    bson_iter_overwrite_decimal128
    bson_iter_overwrite_double
    bson_iter_overwrite_int32
    bson_iter_overwrite_int64
    bson_iter_recurse
    bson_iter_regex
    bson_iter_symbol
    bson_iter_time_t
    bson_iter_timestamp
    bson_iter_timeval
    bson_iter_type
    bson_iter_utf8
    bson_iter_value
    bson_iter_visit_all

Examples
--------

.. code-block:: c

  bson_iter_t iter;

  if (bson_iter_init (&iter, my_bson_doc)) {
     while (bson_iter_next (&iter)) {
        printf ("Found a field named: %s\n", bson_iter_key (&iter));
     }
  }

.. code-block:: c

  bson_iter_t iter;

  if (bson_iter_init (&iter, my_bson_doc) && bson_iter_find (&iter, "my_field")) {
     printf ("Found the field named: %s\n", bson_iter_key (&iter));
  }

.. code-block:: c

  bson_iter_t iter;
  bson_iter_t sub_iter;

  if (bson_iter_init_find (&iter, my_bson_doc, "mysubdoc") &&
      (BSON_ITER_HOLDS_DOCUMENT (&iter) || BSON_ITER_HOLDS_ARRAY (&iter)) &&
      bson_iter_recurse (&iter, &sub_iter)) {
     while (bson_iter_next (&sub_iter)) {
        printf ("Found key \"%s\" in sub document.\n", bson_iter_key (&sub_iter));
     }
  }

.. code-block:: c

  bson_iter_t iter;

  if (bson_iter_init (&iter, my_doc) &&
      bson_iter_find_descendant (&iter, "a.b.c.d", &sub_iter)) {
     printf ("The type of a.b.c.d is: %d\n", (int) bson_iter_type (&sub_iter));
  }

