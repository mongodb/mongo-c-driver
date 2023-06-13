:man_page: mongoc_collection_create_indexes_with_opts

mongoc_collection_create_indexes_with_opts()
============================================

Synopsis
--------

.. code-block:: c

   typedef struct _mongoc_index_model_t mongoc_index_model_t;
 
   mongoc_index_model_t *
   mongoc_index_model_new (const bson_t *keys, const bson_t *opts);
 
   void mongoc_index_model_destroy (mongoc_index_model_t *model);
 
   bool
   mongoc_collection_create_indexes_with_opts (mongoc_collection_t *collection,
                                               mongoc_index_model_t **models,
                                               size_t n_models,
                                               const bson_t *opts,
                                               bson_t *reply,
                                               bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``models``: An array of ``mongoc_index_model_t *``.
* ``n_models``: The number of ``models``.
* ``opts``: Optional options.
* ``reply``: An optional location for the server reply to the ``createIndexes`` command.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

.. |opts-source| replace:: ``collection``

.. include:: includes/write-opts.txt

Additional options passed in ``opts`` are appended to the ``createIndexes`` command. See the `MongoDB Manual for createIndexes <https://www.mongodb.com/docs/manual/reference/command/createIndexes/>`_ for all supported options.

If no write concern is provided in ``opts``, the collection's write concern is used.

Description
-----------

This function wraps around the `createIndexes <https://www.mongodb.com/docs/manual/reference/command/createIndexes/>`_ command.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

Returns ``true`` if successful. Returns ``false`` and sets ``error`` if there are invalid arguments or a server or network error.

.. seealso::

  | :doc:`create-indexes`.
