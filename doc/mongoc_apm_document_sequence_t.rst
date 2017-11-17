:man_page: mongoc_apm_document_sequence_t

mongoc_apm_document_sequence_t
==============================

Synopsis
--------

.. code-block:: c

  typedef struct {
     int32_t size;
     const char *identifier;
     bson_t **documents;
  } mongoc_apm_document_sequence_t;

Used in Application Performance Monitoring to represent the sequence of BSON documents sent to MongoDB with an "insert", "update", or "delete" command, or to represent the sequence of documents returned by a "find" or "aggregate" command.

See Also
--------

:doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_apm_command_started_get_document_sequences
    mongoc_apm_command_succeeded_get_document_sequences
