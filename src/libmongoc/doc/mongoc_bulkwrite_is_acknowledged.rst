:man_page: mongoc_bulkwrite_is_acknowledged

mongoc_bulkwrite_is_acknowledged()
==================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwrite_is_acknowledged (const mongoc_bulkwrite_t * self);

Description
-----------

Returns ``true`` if the previous call to :symbol:`mongoc_bulkwrite_execute` used an acknowledged write concern.
