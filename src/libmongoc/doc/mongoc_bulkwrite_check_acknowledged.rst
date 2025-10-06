:man_page: mongoc_bulkwrite_check_acknowledged

mongoc_bulkwrite_check_acknowledged()
=====================================

Synopsis
--------

.. code-block:: c

   mongoc_bulkwrite_check_acknowledged_t
   mongoc_bulkwrite_check_acknowledged (const mongoc_bulkwrite_t * self, bson_error_t * error);

Description
-----------

Checks whether or not the previous call to :symbol:`mongoc_bulkwrite_execute` used an acknowledged write concern. If
an error occurred, the ``is_ok`` member of :symbol:`mongoc_bulkwrite_check_acknowledged_t` will be ``false`` and
``error`` will be set.
