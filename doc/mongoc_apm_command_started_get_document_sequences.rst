:man_page: mongoc_apm_command_started_get_document_sequences

mongoc_apm_command_started_get_document_sequences()
===================================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_apm_command_started_get_document_sequences (
     const mongoc_apm_command_started_t *event,
     const mongoc_apm_document_sequence_t **sequences,
     size_t *n_sequences);

Returns the document sequences sent to the server with this command. The "insert", "update", and "delete" commands have one document sequence; for other commands sequences is NULL and the number of sequences is 0.

In MongoDB 3.4 and older, the "insert", "update", and "delete" commands include one document sequence as a BSON array within the command body. Starting in MongoDB 3.6, these commands each send one document sequence as a separate section of the OP_MSG, and the document sequence is not available in the command body. This function retrieves the document sequence sent to any version of MongoDB.

The data is only valid in the scope of the callback that receives this event; copy it if it will be accessed after the callback returns.

Parameters
----------

* ``event``: A :symbol:`mongoc_apm_command_started_t`.
* ``sequences``: An array of pointers to a :symbol:`mongoc_apm_document_sequence_t`.
* ``n_sequences``: The number of document sequences sent with the command.

See Also
--------

:doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`
