:man_page: mongoc_collection_create_index

mongoc_collection_create_index()
================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_create_index (mongoc_collection_t *collection,
                                  const bson_t *keys,
                                  const mongoc_index_opt_t *opt,
                                  bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``keys``: A :symbol:`bson:bson_t`.
* ``opt``: A mongoc_index_opt_t.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

For more information, see :symbol:`mongoc_collection_create_index_with_opts()`. This function is a thin wrapper, passing ``NULL`` in as :symbol:`mongoc_write_concern_t` parameter. This function also creates a local :symbol:`bson:bson_t` to pass in as ``reply`` parameter, destroying it afterward.

