:man_page: mongoc_bulkwrite_updateoneopts_set_arrayfilters

mongoc_bulkwrite_updateoneopts_set_arrayfilters()
=================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwrite_updateoneopts_set_arrayfilters (mongoc_bulkwrite_updateoneopts_t *self, const bson_t *arrayfilters);

Description
-----------

Sets a set of filters specifying to which array elements an update should apply.
