:man_page: mongoc_bulkwriteresult_deletedcount

mongoc_bulkwriteresult_deletedcount()
=====================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_bulkwriteresult_deletedcount (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the total number of documents deleted across all delete operations.
