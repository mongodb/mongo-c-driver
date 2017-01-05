:man_page: mongoc_database_destroy

mongoc_database_destroy()
=========================

Synopsis
--------

.. code-block:: none

  void
  mongoc_database_destroy (mongoc_database_t *database);

Releases all resources associated with ``database``, including freeing the structure.

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t <mongoc_database_t>`.

