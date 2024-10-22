:man_page: mongoc_collection_command

mongoc_collection_command()
===========================

.. warning::
   .. deprecated:: 1.29.0

      This function is deprecated and should not be used in new code.

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_collection_command (mongoc_collection_t *collection,
                             mongoc_query_flags_t flags,
                             uint32_t skip,
                             uint32_t limit,
                             uint32_t batch_size,
                             const bson_t *command,
                             const bson_t *fields,
                             const mongoc_read_prefs_t *read_prefs)
     BSON_GNUC_WARN_UNUSED_RESULT BSON_GNUC_DEPRECATED_FOR (mongoc_collection_command_simple);

This function is superseded by :symbol:`mongoc_collection_command_with_opts()`, :symbol:`mongoc_collection_read_command_with_opts()`, :symbol:`mongoc_collection_write_command_with_opts()`, and :symbol:`mongoc_collection_read_write_command_with_opts()`.

.. include:: includes/not-retryable-read.txt

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``flags``: Unused.
* ``skip``: Unused.
* ``limit``: Unused.
* ``batch_size``: Unused.
* ``command``: A :symbol:`bson:bson_t` containing the command to execute.
* ``fields``: Unused.
* ``read_prefs``: An optional :symbol:`mongoc_read_prefs_t`. Otherwise, the command uses mode ``MONGOC_READ_PRIMARY``.

Returns
-------

.. include:: includes/returns-cursor.txt
