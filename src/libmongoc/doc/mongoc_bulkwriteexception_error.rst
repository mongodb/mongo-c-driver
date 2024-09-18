:man_page: mongoc_bulkwriteexception_error

mongoc_bulkwriteexception_error()
=================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self, bson_error_t *error);

Description
-----------

Returns true and sets ``error`` if there was a top-level error. Returns false if there was no top-level error.
