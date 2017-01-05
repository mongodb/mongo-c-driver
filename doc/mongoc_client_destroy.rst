:man_page: mongoc_client_destroy

mongoc_client_destroy()
=======================

Synopsis
--------

.. code-block:: none

  void
  mongoc_client_destroy (mongoc_client_t *client);

Release all resources associated with ``client`` and free the structure.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t <mongoc_client_t>`.

