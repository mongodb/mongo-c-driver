:man_page: mongoc_bulkwriteresult_insertresults

mongoc_bulkwriteresult_insertresults()
======================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteresult_insertresults (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the results of each individual insert operation that was successfully performed. Example:

.. code:: json

   {
     "0" : { "insertedId" : "foo" },
     "1" : { "insertedId" : "bar" }
   }

Returns NULL if verbose results were not requested. Request verbose results with
:symbol:`mongoc_bulkwriteopts_set_verboseresults`.
