:man_page: mongoc_bulkwriteexception_writeconcernerrors

mongoc_bulkwriteexception_writeconcernerrors()
==============================================

Synopsis
--------

.. code-block:: c

   const bson_t *
   mongoc_bulkwriteexception_writeconcernerrors (const mongoc_bulkwriteexception_t *self);

Description
-----------

Returns a BSON array of write concern errors. Example:

.. code:: json

   [
      { "code" : 123, "message" : "foo", "details" : {  } },
      { "code" : 456, "message" : "bar", "details" : {  } }
   ]

Returns an empty array if there are no write concern errors.
