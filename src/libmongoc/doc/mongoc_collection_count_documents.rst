:man_page: mongoc_collection_count_documents

mongoc_collection_count_documents()
===================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_collection_count_documents (mongoc_collection_t *collection,
                                      const bson_t *filter,
                                      const bson_t *opts,
                                      const mongoc_read_prefs_t *read_prefs,
                                      bson_t *reply,
                                      bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``filter``: A :symbol:`bson:bson_t` containing the filter.
* ``opts``: A :symbol:`bson:bson_t`, ``NULL`` to ignore.
* ``read_prefs``: A :symbol:`mongoc_read_prefs_t` or ``NULL``.
* ``reply``: A location for an uninitialized :symbol:`bson:bson_t` to store the command reply, ``NULL`` to ignore. If not ``NULL``, ``reply`` will be initialized.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

.. |opts-source| replace:: ``collection``

.. include:: includes/read-opts.txt
* ``skip``: An int specifying how many documents matching the ``query`` should be skipped before counting.
* ``limit``: An int specifying the maximum number of documents to count.
* ``comment``: A :symbol:`bson_value_t` specifying the comment to attach to this command. The comment will appear in log messages, profiler output, and currentOp output. Specifying a non-string value requires MongoDB 4.4 or later.

Description
-----------

This functions executes a count query on ``collection``. In contrast with :symbol:`mongoc_collection_estimated_document_count()`, the count returned is guaranteed to be accurate.

.. include:: includes/retryable-read.txt

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

-1 on failure, otherwise the number of documents counted.

Example
-------

.. code-block:: c

  #include <bson/bson.h>
  #include <mongoc/mongoc.h>
  #include <stdio.h>

  static void
  print_count (mongoc_collection_t *collection, bson_t *filter)
  {
     bson_error_t error;
     int64_t count;
     bson_t* opts = BCON_NEW ("skip", BCON_INT64(5));

     count = mongoc_collection_count_documents (
        collection, filter, opts, NULL, NULL, &error);
     bson_destroy (opts);

     if (count < 0) {
        fprintf (stderr, "Count failed: %s\n", error.message);
     } else {
        printf ("%" PRId64 " documents counted.\n", count);
     }
  }

.. _migrating-from-deprecated-count:

Migrating from deprecated count functions
-----------------------------------------

When migrating to :symbol:`mongoc_collection_count_documents` from the deprecated :symbol:`mongoc_collection_count` or :symbol:`mongoc_collection_count_with_opts`, the following query operators in the filter must be replaced:

+-------------+-------------------------------------+
| Operator    | Replacement                         |
+=============+=====================================+
| $where      | `$expr`_                            |
+-------------+-------------------------------------+
| $near       | `$geoWithin`_ with `$center`_       |
+-------------+-------------------------------------+
| $nearSphere | `$geoWithin`_ with `$centerSphere`_ |
+-------------+-------------------------------------+

$expr requires MongoDB 3.6+

.. _$expr: https://docs.mongodb.com/manual/reference/operator/query/expr/
.. _$geoWithin: https://docs.mongodb.com/manual/reference/operator/query/geoWithin/
.. _$center: https://docs.mongodb.com/manual/reference/operator/query/center/#op._S_center
.. _$centerSphere: https://docs.mongodb.com/manual/reference/operator/query/centerSphere/#op._S_centerSphere

.. seealso::

  | :symbol:`mongoc_collection_estimated_document_count()`

