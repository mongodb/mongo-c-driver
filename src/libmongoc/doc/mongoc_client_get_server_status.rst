:man_page: mongoc_client_get_server_status

mongoc_client_get_server_status()
=================================

.. warning::
   .. deprecated:: 1.10.0

      This function is deprecated and should not be used in new code.

      Run the `serverStatus <https://www.mongodb.com/docs/manual/reference/command/serverStatus/>`_ command directly with :symbol:`mongoc_client_read_command_with_opts()` instead.

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_get_server_status (mongoc_client_t *client,
                                   mongoc_read_prefs_t *read_prefs,
                                   bson_t *reply,
                                   bson_error_t *error) BSON_GNUC_DEPRECATED;

Queries the server for the current server status. The result is stored in ``reply``.

``reply`` is always initialized, even in the case of failure. Always call :symbol:`bson:bson_destroy()` to release it.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``read_prefs``: A :symbol:`mongoc_read_prefs_t`.
* ``reply``: A |bson_t-opt-storage-ptr| to contain the results.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

Returns ``true`` if successful. Returns ``false`` and sets ``error`` if there are invalid arguments or a server or network error.

