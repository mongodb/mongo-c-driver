:man_page: mongoac_collection_estimated_document_count

mongoac_collection_estimated_document_count()
=============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   uint64_t mongoac_collection_estimated_document_count(const mongoac_collection_t *coll,
                                                        const mongoac_estimated_document_count_options_t *options,
                                                        mongoac_error_t *error);

Description
-----------

Estimates the number of documents in the collection using collection metadata, via `Collection::estimated_document_count() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.estimated_document_count>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``coll`` is null.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_estimated_document_count_async()`
