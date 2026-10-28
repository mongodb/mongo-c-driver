:man_page: mongoac_collection_insert_many

mongoac_collection_insert_many()
================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_insert_many_result_t *mongoac_collection_insert_many(const mongoac_collection_t *collection,
                                                                mongoac_client_session_t *session,
                                                                const mongoac_bson_view_t *documents,
                                                                uintptr_t count,
                                                                const mongoac_insert_many_options_t *options,
                                                                mongoac_error_t *error);

Description
-----------

Inserts the given ``documents``, an array of ``count`` BSON documents, via `Collection::insert_many() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.insert_many>`_.

The returned :symbol:`mongoac_insert_many_result_t` must be destroyed with :symbol:`mongoac_insert_many_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``documents`` is null or empty.
- Invalid Argument (mongoac): a document in ``documents`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_insert_many_async()`
