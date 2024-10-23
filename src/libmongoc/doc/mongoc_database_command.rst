:man_page: mongoc_database_command

mongoc_database_command()
=========================

.. warning::
   .. deprecated:: 1.29.0

      This function is deprecated and should not be used in new code.

Synopsis
--------

.. code-block:: c

  mongoc_cursor_t *
  mongoc_database_command (mongoc_database_t *database,
                           mongoc_query_flags_t flags,
                           uint32_t skip,
                           uint32_t limit,
                           uint32_t batch_size,
                           const bson_t *command,
                           const bson_t *fields,
                           const mongoc_read_prefs_t *read_prefs)
     BSON_GNUC_WARN_UNUSED_RESULT BSON_GNUC_DEPRECATED_FOR (mongoc_database_command_simple);

This function is superseded by :symbol:`mongoc_database_command_simple()`.

Description
-----------

This function creates a cursor which will execute the command when :symbol:`mongoc_cursor_next` is called on it. The database's read preference, read concern, and write concern are not applied to the command, and :symbol:`mongoc_cursor_next` will not check the server response for a write concern error or write concern timeout.

.. include:: includes/not-retryable-read.txt

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.
* ``flags``: Unused.
* ``skip``: Unused.
* ``limit``: Unused.
* ``batch_size``: Unused.
* ``command``: A :symbol:`bson:bson_t` containing the command.
* ``fields``: Unused.
* ``read_prefs``: An optional :symbol:`mongoc_read_prefs_t`. Otherwise, the command uses mode ``MONGOC_READ_PRIMARY``.

Migrating
---------

:symbol:`mongoc_database_command` is deprecated.

The following example uses :symbol:`mongoc_database_command`:

.. literalinclude:: ../examples/migrating.c
   :language: c
   :dedent: 6
   :start-after: // mongoc_database_command ... before ... begin
   :end-before:  // mongoc_database_command ... before ... end
   :caption: Before

To migrate, use a non-deprecated alternative, like :symbol:`mongoc_database_command_simple`:

.. literalinclude:: ../examples/migrating.c
   :language: c
   :dedent: 6
   :start-after: // mongoc_database_command ... after ... begin
   :end-before:  // mongoc_database_command ... after ... end
   :caption: After

Returns
-------

.. include:: includes/returns-cursor.txt

