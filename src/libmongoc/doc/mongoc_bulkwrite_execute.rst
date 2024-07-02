:man_page: mongoc_bulkwrite_execute

mongoc_bulkwrite_execute()
==========================

Synopsis
--------

.. code-block:: c

   mongoc_bulkwritereturn_t
   mongoc_bulkwrite_execute (mongoc_bulkwrite_t *self, const mongoc_bulkwriteopts_t *opts);

Description
-----------

Executes a :symbol:`mongoc_bulkwrite_t`. Once executed, it is an error to call other functions on ``self``, aside from
:symbol:`mongoc_bulkwrite_destroy`.
