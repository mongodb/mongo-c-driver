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
                             const mongoc_read_prefs_t *read_prefs);

This function is superseded by :symbol:`mongoc_collection_command_simple()`.

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

Migrating
---------

:symbol:`mongoc_collection_command` is deprecated.

The following example uses :symbol:`mongoc_collection_command`:

.. literalinclude:: ../examples/migrating.c
   :language: c
   :dedent: 6
   :start-after: // mongoc_collection_command ... before ... begin
   :end-before:  // mongoc_collection_command ... before ... end
   :caption: Before

To migrate, use a non-deprecated alternative, like :symbol:`mongoc_collection_command_simple`:

.. literalinclude:: ../examples/migrating.c
   :language: c
   :dedent: 6
   :start-after: // mongoc_collection_command ... after ... begin
   :end-before:  // mongoc_collection_command ... after ... end
   :caption: After

Returns
-------

.. include:: includes/returns-cursor.txt
