:man_page: mongoc_collection_ensure_index

mongoc_collection_ensure_index()
================================

.. warning::
   .. deprecated:: 1.8.0

      See `Manage Collection Indexes <manage-collection-indexes_>`_ for alternatives.

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_ensure_index (mongoc_collection_t *collection,
                                  const bson_t *keys,
                                  const mongoc_index_opt_t *opt,
                                  bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``keys``: A :symbol:`bson:bson_t`.
* ``opt``: A mongoc_index_opt_t.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Errors
------

Errors are propagated via the ``error`` parameter.

