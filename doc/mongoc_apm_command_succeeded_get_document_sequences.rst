:man_page: mongoc_apm_command_succeeded_get_document_sequences

mongoc_apm_command_succeeded_get_document_sequences()
=====================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_apm_command_succeeded_get_document_sequences (
     const mongoc_apm_command_succeeded_t *event,
     const mongoc_apm_document_sequence_t **sequences,
     size_t *n_sequences);

Returns the document sequences sent by the server. Query-like commands such as "find" and "aggregate" return one document sequence, for other commands the document sequence is NULL and the number of documents is 0.

In MongoDB 3.6 and older, all replies include their document sequences as BSON arrays within the reply body. Starting in MongoDB 3.8, document sequences are sent as separate sections of the OP_MSG, and are not available in the reply body. This function retrieves the document sequences sent by any version of MongoDB server.

The data is only valid in the scope of the callback that receives this event; copy it if it will be accessed after the callback returns.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_command_succeeded_t`.
* ``sequences``: An array of pointers to a :symbol:`mongoc_apm_document_sequence_t`.
* ``n_sequences``: The number of document sequences sent with the command.

See Also
--------

:doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`
