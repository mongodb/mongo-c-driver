:man_page: mongoc_change_stream_t

mongoc_change_stream_t
======================

Synopsis
--------

.. code-block:: c

   #include <mongoc.h>

   typedef struct _mongoc_change_stream_t mongoc_change_stream_t;

:symbol:`mongoc_change_stream_t` is a handle to a change stream. A collection
change stream can be obtained using :symbol:`mongoc_collection_watch`.

It is recommended to use a :symbol:`mongoc_change_stream_t` and its functions instead of a raw aggregation with a ``$changeStream`` stage. For more information see the `MongoDB Manual Entry on Change Streams <http://dochub.mongodb.org/core/changestreams>`_.

Example
-------
.. literalinclude:: ../examples/example-collection-watch.c
   :language: c
   :caption: example-collection-watch.c

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_watch
    mongoc_database_watch
    mongoc_collection_watch
    mongoc_change_stream_next
    mongoc_change_stream_error_document
    mongoc_change_stream_destroy
