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

Unlike :symbol:`mongoac_collection_count_documents()`, this operation reads collection metadata rather than scanning documents, so it is faster but the result may not reflect recent changes. A non-existing collection counts as zero documents.

.. note::

   Due to an oversight in MongoDB 5.0.0-5.0.7, the ``count`` command is not included in version 1 of the Stable API. Users of the Stable API are recommended to upgrade their cluster to 5.0.8+ to avoid errors.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``coll`` is null.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_estimated_document_count_async()`
