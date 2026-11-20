:man_page: mongoac_collection_update_many_document

mongoac_collection_update_many_document()
=========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_update_result_t *mongoac_collection_update_many_document(const mongoac_collection_t *collection,
                                                                    mongoac_client_session_t *session,
                                                                    mongoac_bson_view_t filter,
                                                                    mongoac_bson_view_t update,
                                                                    const mongoac_update_options_t *options,
                                                                    mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Updates all documents matching the given ``filter`` with the given ``update`` document via `Collection::update_many() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.update_many>`_.

The ``update`` document must contain only update operators (e.g. ``{"$set": {"x": 1}}``).

To update with an aggregation pipeline, see :symbol:`mongoac_collection_update_many_pipeline()`.

The returned :symbol:`mongoac_update_result_t` must be destroyed with :symbol:`mongoac_update_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Invalid Argument (mongoac): ``update`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_update_many_document_async()`
