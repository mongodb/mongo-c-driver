:man_page: mongoc_bulkwriteopts_set_writeconcern

mongoc_bulkwriteopts_set_writeconcern()
=======================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_writeconcern (mongoc_bulkwriteopts_t *self, const mongoc_write_concern_t *writeconcern);

Description
-----------

``writeconcern`` is the write concern to use for this bulk write. If a write concern is not set, defaults to the write
concern set on the :symbol:`mongoc_client_t` passed in :symbol:`mongoc_client_bulkwrite_new`.
