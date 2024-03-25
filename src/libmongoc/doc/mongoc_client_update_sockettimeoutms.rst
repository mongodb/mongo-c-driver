:man_page: mongoc_client_update_sockettimeoutms

mongoc_client_update_sockettimeoutms()
=======================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_update_sockettimeoutms (mongoc_client_t *client, const uint32_t timeoutms);

Change the sockettimeoutms of the mongoc_cluster_t object associated with the given mongoc_client_t object.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``timeoutms``: The requested timeout value in milliseconds.

