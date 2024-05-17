:man_page: mongoc_bulkwriteresult_deleteresults

mongoc_bulkwriteresult_deleteresults()
======================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteresult_deleteresults (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the results of each individual delete operation that was successfully performed. Example:

.. code:: json

   {
     "0" : { "deletedCount" : 1 },
     "1" : { "deletedCount" : 2 }
   }

Returns NULL if verbose results were not requested. Request verbose results with
:symbol:`mongoc_bulkwriteopts_set_verboseresults`.
