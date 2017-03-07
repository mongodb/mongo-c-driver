:man_page: mongoc_collection_validate

mongoc_collection_validate()
============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_validate (mongoc_collection_t *collection,
                              const bson_t *options,
                              bson_t *reply,
                              bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``options``: A :symbol:`bson:bson_t`.
* ``reply``: An optional location for a :symbol:`bson:bson_t`.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

This function is a helper function to execute the ``validate`` MongoDB command.

Currently, the only supported options are ``full``, which is a boolean and ``scandata``, also a boolean.

See the MongoDB documentation for more information on this command.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

true if successful, otherwise false and error is set.

``reply`` is always initialized if it's not ``NULL`` and must be destroyed with :symbol:`bson:bson_destroy()`.

