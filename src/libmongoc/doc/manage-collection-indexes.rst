:man_page: mongoc_manage_collection_indexes

Manage Collection Indexes
=========================

To create indexes on a MongoDB collection, use :symbol:`mongoc_collection_create_indexes_with_opts`:

.. literalinclude:: ../examples/example-manage-collection-indexes.c
   :language: c
   :start-after: // Create an index ... begin
   :end-before: // Create an index ... end
   :dedent: 6

To list indexes, use :symbol:`mongoc_collection_find_indexes_with_opts`:

.. literalinclude:: ../examples/example-manage-collection-indexes.c
   :language: c
   :start-after: // List indexes ... begin
   :end-before: // List indexes ... end
   :dedent: 6

To drop an index, use :symbol:`mongoc_collection_drop_index_with_opts`. The index name may be obtained from the ``keys`` document with :symbol:`mongoc_collection_keys_to_index_string`:

.. literalinclude:: ../examples/example-manage-collection-indexes.c
   :language: c
   :start-after: // Drop an index ... begin
   :end-before: // Drop an index ... end
   :dedent: 6

For a full example, see `example-manage-collection-indexes.c <https://github.com/mongodb/mongo-c-driver/blob/master/src/libmongoc/examples/example-manage-collection-indexes.c>`_.

Manage Atlas Search Indexes
---------------------------

To create an Atlas Search Index, use the ``createSearchIndexes`` command:

.. literalinclude:: ../examples/example-manage-search-indexes.c
   :language: c
   :start-after: // Create an Atlas Search Index ... begin
   :end-before: // Create an Atlas Search Index ... end
   :dedent: 6

To list Atlas Search Indexes, use the ``$listSearchIndexes`` aggregation stage:

.. literalinclude:: ../examples/example-manage-search-indexes.c
   :language: c
   :start-after: // List Atlas Search Indexes ... begin
   :end-before: // List Atlas Search Indexes ... end
   :dedent: 6

To update an Atlas Search Index, use the ``updateSearchIndex`` command:

.. literalinclude:: ../examples/example-manage-search-indexes.c
   :language: c
   :start-after: // Update an Atlas Search Index ... begin
   :end-before: // Update an Atlas Search Index ... end
   :dedent: 6

To drop an Atlas Search Index, use the ``dropSearchIndex`` command:

.. literalinclude:: ../examples/example-manage-search-indexes.c
   :language: c
   :start-after: // Drop an Atlas Search Index ... begin
   :end-before: // Drop an Atlas Search Index ... end
   :dedent: 6

For a full example, see `example-manage-search-indexes.c <https://github.com/mongodb/mongo-c-driver/blob/master/src/libmongoc/examples/example-manage-search-indexes.c>`_.
