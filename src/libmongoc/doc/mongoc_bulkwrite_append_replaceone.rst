:man_page: mongoc_bulkwrite_append_replaceone

mongoc_bulkwrite_append_replaceone()
====================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_append_replaceone (mongoc_bulkwrite_t *self,
                                       const char *ns,
                                       const bson_t *filter,
                                       const bson_t *replacement,
                                       const mongoc_bulkwrite_replaceoneopts_t *opts /* May be NULL */,
                                       bson_error_t *error);

Description
-----------

Adds a replace operation for the namespace ``ns``. Returns true on success. Returns false and sets ``error`` if an
error occured.
