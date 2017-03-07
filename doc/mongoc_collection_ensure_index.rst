:man_page: mongoc_collection_ensure_index

mongoc_collection_ensure_index()
================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_ensure_index (mongoc_collection_t *collection,
                                  const bson_t *keys,
                                  const mongoc_index_opt_t *opt,
                                  bson_error_t *error)
     BSON_GNUC_DEPRECATED_FOR (mongoc_collection_create_index);

Deprecated
----------

.. warning::

  This function is deprecated and should not be used in new code.

Please use :symbol:`mongoc_collection_create_index()` instead.

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``keys``: A :symbol:`bson:bson_t`.
* ``opt``: A mongoc_index_opt_t.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

