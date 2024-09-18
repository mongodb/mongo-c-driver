:man_page: mongoc_bulkwriteopts_set_ordered

mongoc_bulkwriteopts_set_ordered()
==================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_bulkwriteopts_set_ordered (mongoc_bulkwriteopts_t *self, bool ordered);

Description
-----------

``ordered`` specifies whether the operations in this bulk write should be executed in the order in which they were
specified. If false, writes will continue to be executed if an individual write fails. If true, writes will stop
executing if an individual write fails.

By default, bulk writes are ordered.
