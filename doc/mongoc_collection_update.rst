:man_page: mongoc_collection_update

mongoc_collection_update()
==========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_collection_update (mongoc_collection_t *collection,
                            mongoc_update_flags_t flags,
                            const bson_t *selector,
                            const bson_t *update,
                            const mongoc_write_concern_t *write_concern,
                            bson_error_t *error);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.
* ``flags``: A bitwise or of :symbol:`mongoc_update_flags_t`.
* ``selector``: A :symbol:`bson:bson_t` containing the query to match documents for updating.
* ``update``: A :symbol:`bson:bson_t` containing the update to perform.
* ``write_concern``: A :symbol:`mongoc_write_concern_t`.
* ``error``: An optional location for a :symbol:`bson_error_t <errors>` or ``NULL``.

Description
-----------

This function shall update documents in ``collection`` that match ``selector``.

By default, updates only a single document. Set flags to ``MONGOC_UPDATE_MULTI_UPDATE`` to update multiple documents.

Errors
------

Errors are propagated via the ``error`` parameter.

Returns
-------

true if successful, otherwise false and error is set.

A write concern timeout or write concern error is considered a failure.

