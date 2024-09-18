:man_page: mongoc_bulkwriteresult_upsertedcount

mongoc_bulkwriteresult_upsertedcount()
======================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_bulkwriteresult_upsertedcount (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the total number of documents upserted across all update operations.
