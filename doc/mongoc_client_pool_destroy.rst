:man_page: mongoc_client_pool_destroy

mongoc_client_pool_destroy()
============================

Synopsis
--------

.. code-block:: none

  void
  mongoc_client_pool_destroy (mongoc_client_pool_t *pool);

Release all resources associated with ``pool``, including freeing the structure.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t <mongoc_client_pool_t>`.

