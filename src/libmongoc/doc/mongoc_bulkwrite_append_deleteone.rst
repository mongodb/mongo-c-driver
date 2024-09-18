:man_page: mongoc_bulkwrite_append_deleteone

mongoc_bulkwrite_append_deleteone()
===================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_append_deleteone (mongoc_bulkwrite_t *self,
                                      const char *ns,
                                      const bson_t *filter,
                                      const mongoc_bulkwrite_deleteoneopts_t *opts /* May be NULL */,
                                      bson_error_t *error);

Description
-----------

Adds a single-document delete into the namespace ``ns``. Returns true on success. Returns false and sets ``error`` if an
error occured.
