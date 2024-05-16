:man_page: mongoc_bulkwriteopts_set_let

mongoc_bulkwriteopts_set_let()
==============================

Synopsis
--------

.. code-block:: c

    void
    mongoc_bulkwriteopts_set_let (mongoc_bulkwriteopts_t *self, const bson_t *let);

Description
-----------

``let`` is a map of parameter names and values to apply to all operations within the bulk write. Value must be constant
or closed expressions that do not reference document fields. Parameters can then be accessed as variables in an
aggregate expression context (e.g. "$$var").
