:man_page: mongoc_client_bulkwrite_new

mongoc_client_bulkwrite_new()
=============================

Synopsis
--------

.. code-block:: c

   mongoc_bulkwrite_t *
   mongoc_client_bulkwrite_new (mongoc_client_t *self);
   

Description
-----------

Returns a new :symbol:`mongoc_bulkwrite_t`. Free with :symbol:`mongoc_bulkwrite_destroy()`.
