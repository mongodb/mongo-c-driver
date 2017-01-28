:man_page: mongoc_client_command

mongoc_client_command()
=======================

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_client_command (mongoc_client_t *client,
                         const char *db_name,
                         mongoc_query_flags_t flags,
                         uint32_t skip,
                         uint32_t limit,
                         uint32_t batch_size,
                         const bson_t *query,
                         const bson_t *fields,
                         const mongoc_read_prefs_t *read_prefs);

This function creates a cursor which will execute the command when :symbol:`mongoc_cursor_next` is called on it. The client's read preference, read concern, and write concern are not applied to the command, and :symbol:`mongoc_cursor_next` will not check the server response for a write concern error or write concern timeout.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``db_name``: The name of the database to run the command on.
* ``flags``: A :symbol:`mongoc_query_flags_t`.
* ``skip``: The number of result documents to skip.
* ``limit``: The maximum number of documents to return.
* ``batch_size``: The batch size of documents to return from the MongoDB server.
* ``query``: A :symbol:`bson:bson_t` containing the command specification.
* ``fields``: An optional :symbol:`bson:bson_t` containing the fields to return in result documents.
* ``read_prefs``: An optional :symbol:`mongoc_read_prefs_t`. Otherwise, the command uses mode ``MONGOC_READ_PRIMARY``.

Returns
-------

A :symbol:`mongoc_cursor_t`.

The cursor should be freed with :symbol:`mongoc_cursor_destroy()`.

