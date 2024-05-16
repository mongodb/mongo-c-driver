:man_page: mongoc_bulkwriteopts_set_serverid

mongoc_bulkwriteopts_set_serverid()
===================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_serverid (mongoc_bulkwriteopts_t *self, uint32_t serverid);

Description
-----------

Identifies which server to perform the operation. Intended for use by wrapping drivers that select a server before
running the operation.
