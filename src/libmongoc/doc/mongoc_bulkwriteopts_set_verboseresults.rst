:man_page: mongoc_bulkwriteopts_set_verboseresults

mongoc_bulkwriteopts_set_verboseresults()
=========================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_verboseresults (mongoc_bulkwriteopts_t *self, bool verboseresults);

Description
-----------

If ``verboseresults`` is true, detailed results for each successful operation will be included in the returned results.

By default, verbose results are not included.

Verbose results can be obtained with the following:

- :symbol:`mongoc_bulkwriteresult_insertresults`
- :symbol:`mongoc_bulkwriteresult_updateresults`
- :symbol:`mongoc_bulkwriteresult_deleteresults`
