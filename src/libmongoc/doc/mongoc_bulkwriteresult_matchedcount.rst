:man_page: mongoc_bulkwriteresult_matchedcount

mongoc_bulkwriteresult_matchedcount()
=====================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_bulkwriteresult_matchedcount (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the total number of documents matched across all update operations.
