:man_page: mongoc_bulkwrite_serverid_t

mongoc_bulkwrite_serverid_t
===========================

Synopsis
--------

.. code-block:: c

   typedef struct {
      bool is_ok;        // true if no error
      uint32_t serverid; // the server ID last used in `mongoc_bulkwrite_execute`
   } mongoc_bulkwrite_serverid_t;

Description
-----------

:symbol:`mongoc_bulkwrite_serverid_t` is returned by :symbol:`mongoc_bulkwrite_serverid`.

``is_ok`` is ``false`` if there was no previous call to :symbol:`mongoc_bulkwrite_execute` or if execution failed.

``serverid`` is the server ID last used in :symbol:`mongoc_bulkwrite_execute`.
