:man_page: mongoc_bulkwrite_set_client

mongoc_bulkwrite_set_client()
=============================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwrite_set_client (mongoc_bulkwrite_t *self, mongoc_client_t *client);

Description
-----------

Sets the client that will be used to execute the :symbol:`mongoc_bulkwrite_t`.
