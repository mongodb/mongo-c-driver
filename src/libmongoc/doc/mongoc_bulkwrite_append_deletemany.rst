:man_page: mongoc_bulkwrite_append_deletemany

mongoc_bulkwrite_append_deletemany()
====================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_append_deletemany (mongoc_bulkwrite_t *self,
                                       const char *ns,
                                       const bson_t *filter,
                                       const mongoc_bulkwrite_deletemanyopts_t *opts /* May be NULL */,
                                       bson_error_t *error);

Description
-----------

Adds a multi-document delete into the namespace ``ns``. Returns true on success. Returns false and sets ``error`` if an
error occured.
