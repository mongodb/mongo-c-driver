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

Starting and Resuming
`````````````````````

All ``watch`` functions accept two options to indicate where a change stream should start returning changes from: ``startAtOperationTime`` and ``resumeAfter``.

All changes returned by :symbol:`mongoc_change_stream_next` include a resume token in the ``_id`` field. This resume token is automatically cached in libmongoc.
In the event of an error, libmongoc attempts to recreate the change stream starting where it left off by passing the resume token.
libmongoc only attempts to resume once, but client applications can cache this resume token and use it for their own resume logic by passing it as the option ``resumeAfter``.

Additionally, change streams can start returning changes at an operation time by using the ``startAtOperationTime`` field. This can be the timestamp returned in the ``operationTime`` field of a command reply.

``startAtOperationTime`` and ``resumeAfter`` are mutually exclusive options. Setting them both will result in a server error.

The following example implements custom resuming logic, persisting the resume token in a file.

.. literalinclude:: ../examples/example-resume.c
   :language: c
   :caption: example-resume.c

The following example shows using ``startAtOperationTime`` to synchronize a change stream with another operation.

.. literalinclude:: ../examples/example-start-at-optime.c
   :language: c
   :caption: example-start-at-optime.c


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
