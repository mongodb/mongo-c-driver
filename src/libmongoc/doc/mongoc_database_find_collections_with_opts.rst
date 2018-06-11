:man_page: mongoc_database_find_collections_with_opts

mongoc_database_find_collections_with_opts()
============================================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_database_find_collections_with_opts (mongoc_database_t *database,
                                              const bson_t *opts);

Fetches a cursor containing documents, each corresponding to a collection on this database.

To get collection names only, use :symbol:`mongoc_database_get_collection_names_with_opts`.

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.
* ``opts``: Optional :symbol:`bson:bson_t` which may contain a subdocument named "filter", and may contain additional options.

Errors
------

Use :symbol:`mongoc_cursor_error` on the returned cursor to check for errors.

Returns
-------

A cursor where each result corresponds to the server's representation of a collection in this database.

The cursor functions :symbol:`mongoc_cursor_set_limit`, :symbol:`mongoc_cursor_set_batch_size`, and :symbol:`mongoc_cursor_set_max_await_time_ms` have no use on the returned cursor.

Examples
--------

.. code-block:: c

  {
     bson_t opts = BSON_INITIALIZER;
     bson_t name_filter;
     const bson_t *doc;
     bson_iter_t iter;
     bson_error_t error;

     BSON_APPEND_DOCUMENT_BEGIN (&opts, "filter", &name_filter);
     /* find collections with names like "abbbbc" */
     BSON_APPEND_REGEX (&name_filter, "name", "ab+c", NULL);
     bson_append_document_end (&opts, &name_filter);

     cursor = mongoc_database_find_collections_with_opts (database, &opts);
     while (mongoc_cursor_next (cursor, &doc)) {
        bson_iter_init_find (&iter, doc, "name");
        printf ("found collection: %s\n", bson_iter_utf8 (&iter, NULL));
     }

     if (mongoc_cursor_error (cursor, &error))
        fprintf (stderr, "%s\n", error.msg);
     }

     mongoc_cursor_destroy (cursor);
     bson_destroy (&opts);
  }

See Also
--------

:symbol:`mongoc_database_get_collection_names_with_opts()`
