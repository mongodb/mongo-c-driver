:man_page: mongoc_bulkwriteexception_writeerrors

mongoc_bulkwriteexception_writeerrors()
=======================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteexception_writeerrors (const mongoc_bulkwriteexception_t *self);

Description
-----------

Returns a BSON document mapping model indexes to write errors. Example:

.. code:: json

   {
     "0" : { "code" : 123, "message" : "foo", "details" : {  } },
     "1" : { "code" : 456, "message" : "bar", "details" : {  } }
   }

Returns an empty document if there are no write errors.
