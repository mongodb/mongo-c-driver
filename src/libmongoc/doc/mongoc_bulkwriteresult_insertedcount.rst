:man_page: mongoc_bulkwriteresult_insertedcount

mongoc_bulkwriteresult_insertedcount()
======================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_bulkwriteresult_insertedcount (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the total number of documents inserted across all insert operations.
