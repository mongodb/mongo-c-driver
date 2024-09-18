:man_page: mongoc_bulkwriteresult_updateresults

mongoc_bulkwriteresult_updateresults()
======================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteresult_updateresults (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the results of each individual update operation that was successfully performed. Example:

.. code:: json

   {
     "0" : { "matchedCount" : 2, "modifiedCount" : 2 },
     "1" : { "matchedCount" : 1, "modifiedCount" : 0, "upsertedId" : "foo" }
   }

Returns NULL if verbose results were not requested. Request verbose results with
:symbol:`mongoc_bulkwriteopts_set_verboseresults`.
