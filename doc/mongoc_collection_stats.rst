:man_page: mongoc_collection_stats

mongoc_collection_stats()
=========================

Synopsis
--------

.. code-block:: none

  bool
  mongoc_collection_stats (mongoc_collection_t *collection,
                           const bson_t        *options,
                           bson_t              *reply,
                           bson_error_t        *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t <mongoc_collection_t>`.
* ``options``: An optional :symbol:`bson_t <bson:bson_t>` containing extra options to pass to the ``collStats`` command.
* ``reply``: An uninitialized :symbol:`bson_t <bson:bson_t>` to store the result.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

Run the ``collStats`` command to retrieve statistics about the collection.

The command uses the :symbol:`mongoc_read_prefs_t <mongoc_read_prefs_t>` set on ``collection``.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

True if successful.

``reply`` is always initialized and must be freed with :symbol:`bson_destroy <bson:bson_destroy>`.

