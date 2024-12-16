:man_page: mongoc_bulkwrite_updateoneopts_set_sort

mongoc_bulkwrite_updateoneopts_set_sort()
=========================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwrite_updateoneopts_set_sort (mongoc_bulkwrite_updateoneopts_t *self, bson_t* sort);

Description
-----------

``sort`` specifies a sorting order if the query matches multiple documents.
The first document matched by the sort order will be updated.
This option is only sent if the caller explicitly provides a value.
