:man_page: mongoac_collection_count_documents

mongoac_collection_count_documents()
====================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   uint64_t mongoac_collection_count_documents(const mongoac_collection_t *coll,
                                               mongoac_client_session_t *session,
                                               mongoac_bson_view_t filter,
                                               const mongoac_count_options_t *options,
                                               mongoac_error_t *error);

Description
-----------

Gets the number of documents matching the given ``filter`` via `Collection::count_documents() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.count_documents>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``coll`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_count_documents_async()`
