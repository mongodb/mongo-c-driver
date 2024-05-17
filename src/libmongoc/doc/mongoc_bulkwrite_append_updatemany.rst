:man_page: mongoc_bulkwrite_append_updatemany

mongoc_bulkwrite_append_updatemany()
====================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_append_updatemany (mongoc_bulkwrite_t *self,
                                       const char *ns,
                                       const bson_t *filter,
                                       const bson_t *update,
                                       const mongoc_bulkwrite_updatemanyopts_t *opts /* May be NULL */,
                                       bson_error_t *error);

Description
-----------

Adds a multi-document update for the namespace ``ns``. Returns true on success. Returns false and sets ``error`` if an
error occured.
