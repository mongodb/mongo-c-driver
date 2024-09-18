:man_page: mongoc_bulkwriteresult_serverid

mongoc_bulkwriteresult_serverid()
=================================

Synopsis
--------

.. code-block:: c

   uint32_t
   mongoc_bulkwriteresult_serverid (const mongoc_bulkwriteresult_t *self);

Description
-----------

Returns the most recently selected server. The returned value may differ from a ``serverid`` previously set with
:symbol:`mongoc_bulkwriteopts_set_serverid` if a retry occurred. Intended for use by wrapping drivers that select a
server before running the operation.
