:man_page: mongoc_client_get_handshake_description

mongoc_client_get_handshake_description()
=========================================

Synopsis
--------

.. code-block:: c

   mongoc_server_description_t *
   mongoc_client_get_handshake_description (mongoc_client_t *client,
                                            uint32_t server_id,
                                            bson_t *opts,
                                            bson_error_t *error);

Returns a description constructed from the initial handshake response to a server.

Description
-----------

:symbol:`mongoc_client_get_handshake_description` is distinct from :symbol:`mongoc_client_get_server_description`. :symbol:`mongoc_client_get_server_description` returns a server description constructed from monitoring, which may differ from the server description constructed from the connection handshake.

:symbol:`mongoc_client_get_handshake_description` does not attempt to establish a connection to the server if a connection was not already established. It will complete authentication on a single-threaded client if the connection has not yet been used by an operation.

To establish a connection on a single-threaded client, ensure the server is selectable (the monitoring connection is the only connection made to the server). This can be done with a function like :symbol:`mongoc_client_select_server`, or any function which performs server selection.

To establish a connection on a pooled client, send a command (e.g. ping) to the server.

Use this function only for building a language driver that wraps the C Driver. When writing applications in C, higher-level functions automatically select a suitable server.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``server_id``: The ID of the server. This can be obtained from the server description of :symbol:`mongoc_client_select_server`.
* ``opts``: Unused. Pass ``NULL``.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Returns
-------

A :symbol:`mongoc_server_description_t` that must be freed with :symbol:`mongoc_server_description_destroy`. If a connection has not been successfully established to a server, returns NULL and ``error`` is filled out.


See Also
--------

- :symbol:`mongoc_client_select_server` To select a server from read preferences.
- :symbol:`mongoc_client_get_server_description` To obtain the server description from monitoring for a server.
