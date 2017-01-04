:man_page: bson_t

bson_t
======

BSON Document Abstraction

Synopsis
--------

.. code-block:: c

  #include <bson.h>

  BSON_ALIGNED_BEGIN (128)
  typedef struct {
     uint32_t flags;       /* Internal flags for the bson_t. */
     uint32_t len;         /* Length of BSON data. */
     uint8_t padding[120]; /* Padding for stack allocation. */
  } bson_t BSON_ALIGNED_END (128);

Description
-----------

The :symbol:`bson_t <bson_t>` structure represents a BSON document. This structure manages the underlying BSON encoded buffer. For mutable documents, it can append new data to the document.

Performance Notes
-----------------

The :symbol:`bson_t <bson_t>` structure attepts to use an inline allocation within the structure to speed up performance of small documents. When this internal buffer has been exhausted, a heap allocated buffer will be dynamically allocated. Therefore, it is essential to call :symbol:`bson_destroy() <bson_destroy>` on allocated documents.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    bson_append_array
    bson_append_array_begin
    bson_append_array_end
    bson_append_binary
    bson_append_bool
    bson_append_code
    bson_append_code_with_scope
    bson_append_date_time
    bson_append_dbpointer
    bson_append_decimal128
    bson_append_document
    bson_append_document_begin
    bson_append_document_end
    bson_append_double
    bson_append_int32
    bson_append_int64
    bson_append_iter
    bson_append_maxkey
    bson_append_minkey
    bson_append_now_utc
    bson_append_null
    bson_append_oid
    bson_append_regex
    bson_append_symbol
    bson_append_time_t
    bson_append_timestamp
    bson_append_timeval
    bson_append_undefined
    bson_append_utf8
    bson_append_value
    bson_as_json
    bson_compare
    bson_concat
    bson_copy
    bson_copy_to
    bson_copy_to_excluding
    bson_count_keys
    bson_destroy
    bson_destroy_with_steal
    bson_equal
    bson_get_data
    bson_has_field
    bson_init
    bson_init_from_json
    bson_init_static
    bson_new
    bson_new_from_buffer
    bson_new_from_data
    bson_new_from_json
    bson_reinit
    bson_reserve_buffer
    bson_sized_new
    bson_steal
    bson_validate

Example
-------

.. code-block:: c

  static void
  create_on_heap (void)
  {
     bson_t *b = bson_new ();

     BSON_APPEND_INT32 (b, "foo", 123);
     BSON_APPEND_UTF8 (b, "bar", "foo");
     BSON_APPEND_DOUBLE (b, "baz", 1.23f);

     bson_destroy (b);
  }

