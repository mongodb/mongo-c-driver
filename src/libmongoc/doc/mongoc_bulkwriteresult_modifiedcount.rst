:man_page: mongoc_bulkwriteresult_modifiedcount

mongoc_bulkwriteresult_modifiedcount()
======================================

Synopsis
--------

.. code-block:: c

   int64_t
   mongoc_bulkwriteresult_modifiedcount (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the total number of documents modified across all update operations.
