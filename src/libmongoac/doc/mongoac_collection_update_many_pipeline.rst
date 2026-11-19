:man_page: mongoac_collection_update_many_pipeline

mongoac_collection_update_many_pipeline()
=========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_update_result_t *mongoac_collection_update_many_pipeline(const mongoac_collection_t *collection,
                                                                    mongoac_client_session_t *session,
                                                                    mongoac_bson_view_t filter,
                                                                    mongoac_bson_view_array_t pipeline,
                                                                    const mongoac_update_options_t *options,
                                                                    mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Updates all documents matching the given ``filter`` with the given aggregation ``pipeline`` via `Collection::update_many() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.update_many>`_.

Each document in ``pipeline`` is an aggregation pipeline stage (e.g. ``{"$set": {"x": 1}}``).

To update with an update document, see :symbol:`mongoac_collection_update_many_document()`.

The returned :symbol:`mongoac_update_result_t` must be destroyed with :symbol:`mongoac_update_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Invalid Argument (mongoac): ``pipeline`` is null.
- Invalid Argument (mongoac): a document in ``pipeline`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_update_many_pipeline_async()`
