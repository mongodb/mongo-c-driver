:man_page: mongoc_logging

Logging
=======

The MongoDB C driver has two different types of logging available:

* The original ``mongoc_log`` facility supports freeform string messages that originate from the driver itself or from application code. This has been retroactively termed "unstructured logging".
* A new ``mongoc_structured_log`` facility reports messages from the driver itself using a BSON format defined across driver implementations by the `MongoDB Logging Specification <https://specifications.readthedocs.io/en/latest/logging/logging/>`_.

These two systems are configured and used independently.

.. toctree::
  :titlesonly:
  :maxdepth: 1

  unstructured_log
  structured_log
