:man_page: mongoc_collection_command_with_opts

mongoc_collection_command_with_opts()
=====================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_command_with_opts (
     mongoc_collection_t *collection,
     const bson_t *command,
     const bson_t *opts,
     bson_t *reply,
     bson_error_t *error);


Execute a command on the server, interpreting ``opts`` according to the MongoDB server version. To send a raw command to the server without any of this logic, use :symbol:`mongoc_client_command_simple`.

Collation is applied from ``opts`` (:ref:`see example for the "distinct" command with opts <mongoc_client_read_command_with_opts_example>`). Collation requires MongoDB 3.2 or later, otherwise an error is returned. Read preferences, read concern, and write concern are applied from ``opts``. The write concern is omitted for MongoDB before 3.2.

To target a specific server, include an integer "serverId" field in ``opts`` with an id obtained first by calling :symbol:`mongoc_client_select_server`, then :symbol:`mongoc_server_description_id` on its return value.

``reply`` is always initialized, and must be freed with :symbol:`bson:bson_destroy()`.

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``command``: A :symbol:`bson:bson_t` containing the command specification.
* ``opts``: A :symbol:`bson:bson_t` containing additional options.
* ``reply``: A location for the resulting document.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

Returns ``true`` if successful. Returns ``false`` and sets ``error`` if there are invalid arguments or a server or network error.

The reply is not parsed for a write concern timeout or write concern error.

Example
-------

See the example code for :symbol:`mongoc_client_read_command_with_opts`.

