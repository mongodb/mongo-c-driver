:man_page: mongoac_collection_t

mongoac_collection_t
====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection-fwd.h>

   typedef struct mongoac_collection_t mongoac_collection_t;

   #include <mongoac/collection.h>

Description
-----------

Represents a `mongodb::Collection <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html>`_.

All operations executed with a given collection and its associated objects are spawned and driven with the associated runtime.
The associated runtime may be obtained with :symbol:`mongoac_collection_get_runtime()`.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_collection_destroy
    mongoac_collection_clone

    mongoac_collection_get_client
    mongoac_collection_get_name
    mongoac_collection_get_runtime
    mongoac_collection_get_runtime_address

    mongoac_collection_drop
    mongoac_collection_drop_async

    mongoac_collection_count_documents
    mongoac_collection_count_documents_async

    mongoac_collection_estimated_document_count
    mongoac_collection_estimated_document_count_async
