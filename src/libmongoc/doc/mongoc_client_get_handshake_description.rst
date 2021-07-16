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

Use this function only for building a language driver that wraps the C Driver. When writing applications in C, higher-level functions automatically select a suitable server.

:symbol:`mongoc_client_get_handshake_description` does not attempt to establish a connection to the server if a connection was not already established. If a connection has not been established, this returns ``NULL`` and sets ``error``.

Single-threaded client behavior
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
To ensure a connection has been established ensure that the server is selectable (e.g. by calling :symbol:`mongoc_client_select_server`) or that an operation has successfully run on the server (e.g. by sending "ping").

If the established connection has not completed authentication, calling :symbol:`mongoc_client_get_handshake_description` will complete authentication on the connection.

Single-threaded clients only have one active connection to each server. The one connection is used for both monitoring and application operations. However, the server description returned by :symbol:`mongoc_client_get_handshake_description` may still differ from :symbol:`mongoc_client_get_server_description`. Notably, if connected to a load balanced cluster, the :symbol:`mongoc_client_get_server_description` will describe the load balancer server (:symbol:`mongoc_server_description_type` will return "LoadBalancer"). And :symbol:`mongoc_client_get_handshake_description` will describe the backing server.

Pooled client behavior
^^^^^^^^^^^^^^^^^^^^^^
To ensure a connection has been established ensure an operation has successfully run on a server (e.g. by sending "ping").

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
- :symbol:`mongoc_server_description_type` To obtain the type of server from a server description.
