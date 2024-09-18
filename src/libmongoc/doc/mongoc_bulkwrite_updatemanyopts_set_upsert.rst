:man_page: mongoc_bulkwrite_updatemanyopts_set_upsert

mongoc_bulkwrite_updatemanyopts_set_upsert()
============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwrite_updatemanyopts_set_upsert (mongoc_bulkwrite_updatemanyopts_t *self, bool upsert);

Description
-----------

If ``upsert`` is true, creates a new document if no document matches the query.

The ``upsert`` option is not sent if this function is not called. The server's default value is false.
