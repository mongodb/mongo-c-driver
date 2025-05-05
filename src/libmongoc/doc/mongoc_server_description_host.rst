:man_page: mongoc_server_description_host

mongoc_server_description_host()
================================

Synopsis
--------

.. code-block:: c

  const mongoc_host_list_t *
  mongoc_server_description_host (const mongoc_server_description_t *description);

Parameters
----------

* ``description``: A :symbol:`mongoc_server_description_t`.

Description
-----------

Return the server's host and port. This object is owned by the server description.

Returns
-------

.. versionchanged:: 2.0.0 The return type changed from ``mongoc_host_list_t *`` to ``const mongoc_host_list_t *``.

A reference to the server description's :symbol:`mongoc_host_list_t`.

