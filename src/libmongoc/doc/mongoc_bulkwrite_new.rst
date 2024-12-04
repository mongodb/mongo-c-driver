:man_page: mongoc_bulkwrite_new

mongoc_bulkwrite_new()
======================

Synopsis
--------

.. code-block:: c

   mongoc_bulkwrite_t *
   mongoc_bulkwrite_new (void);

Description
-----------

Returns a new :symbol:`mongoc_bulkwrite_t`. Free with :symbol:`mongoc_bulkwrite_destroy()`.

A client must be assigned with :symbol:`mongoc_bulkwrite_set_client()` prior to execution.
