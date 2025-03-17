:man_page: mongoc_collection_get_last_error

mongoc_collection_get_last_error()
==================================

.. warning::
   .. deprecated:: 1.9.0

    To get write results from write operations, instead use:

    - :symbol:`mongoc_collection_update_one`
    - :symbol:`mongoc_collection_update_many`
    - :symbol:`mongoc_collection_replace_one`
    - :symbol:`mongoc_collection_delete_one`
    - :symbol:`mongoc_collection_delete_many`
    - :symbol:`mongoc_collection_insert_one`
    - :symbol:`mongoc_collection_insert_many`
    - :symbol:`mongoc_bulkwrite_t`
    - :symbol:`mongoc_bulk_operation_t`


Synopsis
--------

.. code-block:: c

  const bson_t *
  mongoc_collection_get_last_error (const mongoc_collection_t *collection);

Parameters
----------

* ``collection``: A :symbol:`mongoc_collection_t`.

Description
-----------

:symbol:`mongoc_collection_get_last_error` returns write results from some operations:

- :symbol:`mongoc_collection_update`
- :symbol:`mongoc_collection_remove`
- :symbol:`mongoc_collection_insert_bulk`
- :symbol:`mongoc_collection_insert`

Returns
-------

A :symbol:`bson:bson_t` that should not be modified or ``NULL``. The returned :symbol:`bson:bson_t` is may be
invalidated by the next operation on ``collection``.

