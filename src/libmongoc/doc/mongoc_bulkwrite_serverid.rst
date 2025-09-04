:man_page: mongoc_bulkwrite_serverid

mongoc_bulkwrite_serverid()
===========================

Synopsis
--------

.. code-block:: c

   mongoc_bulkwrite_serverid_maybe_t
   mongoc_bulkwrite_serverid (const mongoc_bulkwrite_t * self, bson_error_t * error);

Description
-----------

Gets the server ID last used in the previous call to :symbol:`mongoc_bulkwrite_execute`. If an error occured, the
``is_ok`` member of :symbol:`mongoc_bulkwrite_serverid_maybe_t` will be ``false`` and ``error`` will be set.
