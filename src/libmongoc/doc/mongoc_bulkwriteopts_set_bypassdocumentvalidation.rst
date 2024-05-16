:man_page: mongoc_bulkwriteopts_set_bypassdocumentvalidation

mongoc_bulkwriteopts_set_bypassdocumentvalidation()
===================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_bypassdocumentvalidation (mongoc_bulkwriteopts_t *self, bool bypassdocumentvalidation);

Description
-----------

If ``bypassdocumentvalidation`` is true, allows the writes to opt out of document-level validation.

This option is only sent to the server if :symbol:`mongoc_bulkwriteopts_set_bypassdocumentvalidation` is called. The
server's default value is false.
