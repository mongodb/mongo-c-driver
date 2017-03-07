:man_page: mongoc_database_drop_with_opts

mongoc_database_drop_with_opts()
================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_database_drop_with_opts (mongoc_database_t *database,
                                  const bson_t *opts,
                                  bson_error_t *error);

Parameters
----------

* ``database``: A :symbol:`mongoc_database_t`.
* ``opts``: A :symbol:`bson:bson_t` or ``NULL``.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

This function attempts to drop a database on the MongoDB server.

If no write concern is provided in ``command_opts``, the database's write concern is used.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

``true`` if successful, otherwise ``false`` and ``error`` is set.

