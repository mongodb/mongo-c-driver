:man_page: mongoc_bulkwriteopts_set_comment

mongoc_bulkwriteopts_set_comment()
==================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_bulkwriteopts_set_comment (mongoc_bulkwriteopts_t *self, const bson_value_t *comment);

Description
-----------

Enables users to specify an arbitrary comment to help trace the operation through the database profiler, currentOp and
logs.
