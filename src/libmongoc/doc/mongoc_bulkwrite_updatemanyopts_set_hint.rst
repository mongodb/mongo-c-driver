:man_page: mongoc_bulkwrite_updatemanyopts_set_hint

mongoc_bulkwrite_updatemanyopts_set_hint()
==========================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwrite_updatemanyopts_set_hint (mongoc_bulkwrite_updatemanyopts_t *self, const bson_value_t *hint);

Description
-----------

Specifies the index to use. Specify either the index name as a string or the index key pattern. If specified, then the
query system will only consider plans using the hinted index.
