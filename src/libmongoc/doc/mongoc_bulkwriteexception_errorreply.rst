:man_page: mongoc_bulkwriteexception_errorreply

mongoc_bulkwriteexception_errorreply()
======================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteexception_errorreply (const mongoc_bulkwriteexception_t *self);

Description
-----------

Returns a possible server reply related to the error, or an empty document.
