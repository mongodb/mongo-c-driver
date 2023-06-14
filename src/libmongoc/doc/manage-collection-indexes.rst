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
