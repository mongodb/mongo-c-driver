:man_page: mongoc_collection_get_last_error

mongoc_collection_get_last_error()
==================================

.. warning::
   .. deprecated:: 1.9.0

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
- :symbol:`mongoc_collection_delete`
- :symbol:`mongoc_collection_insert_bulk`
- :symbol:`mongoc_collection_insert`

Returns
-------

A :symbol:`bson:bson_t` that should not be modified or ``NULL``.

