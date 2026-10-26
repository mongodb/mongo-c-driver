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

The returned count is accurate, reflecting the documents actually matched (in contrast with an estimated count derived from collection metadata).

Returns the number of documents counted, or ``0`` on failure with ``error`` set, if provided. A count of zero documents is also a valid result (for an empty or non-existing collection) so callers must check ``error`` to distinguish the two.

``filter`` must not be null. An empty filter (``{}``) matches all documents.

The ``filter`` is applied via a ``$match`` stage, so operators that are unsupported by ``$match`` cannot be used, including ``$where`` and ``$near``. See the `server documentation <https://www.mongodb.com/docs/manual/reference/operator/aggregation/match/#match-restrictions>`_ for the full list of restrictions.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``coll`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error (e.g. an unsupported filter operator like ``$where``).
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_count_documents_async()`
