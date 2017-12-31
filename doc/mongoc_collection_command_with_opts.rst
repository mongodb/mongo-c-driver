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
     const mongoc_read_prefs_t *read_prefs,
     const bson_t *opts,
     bson_t *reply,
     bson_error_t *error);

Execute a command on the server, interpreting ``opts`` according to the MongoDB server version. To send a raw command to the server without any of this logic, use :symbol:`mongoc_client_command_simple`.

Read preferences are applied from ``read_prefs`` or else from ``collection``. Collation, read concern, and write concern are applied from ``opts`` (:ref:`see example for the "distinct" command with opts <mongoc_client_read_command_with_opts_example>`). The write concern is ignored for MongoDB before 3.4.

``reply`` is always initialized, and must be freed with :symbol:`bson:bson_destroy()`.

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``command``: A :symbol:`bson:bson_t` containing the command specification.
* ``read_prefs``: An optional :symbol:`mongoc_read_prefs_t`.
* ``opts``: A :symbol:`bson:bson_t` containing additional options.
* ``reply``: A location for the resulting document.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

.. include:: includes/read-write-opts.txt

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

