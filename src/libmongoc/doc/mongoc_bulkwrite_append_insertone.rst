:man_page: mongoc_bulkwrite_append_insertone

mongoc_bulkwrite_append_insertone()
===================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_append_insertone (mongoc_bulkwrite_t *self,
                                      const char *ns,
                                      const bson_t *document,
                                      const mongoc_bulkwrite_insertoneopts_t *opts /* May be NULL */,
                                      bson_error_t *error);

Description
-----------

Adds a document to insert into the namespace ``ns``. Returns true on success. Returns false and sets ``error`` if an
error occured.
