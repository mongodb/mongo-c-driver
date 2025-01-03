:man_page: mongoc_structured_log_opts_destroy

mongoc_structured_log_opts_destroy()
====================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_structured_log_opts_destroy (mongoc_structured_log_opts_t *opts);

Parameters
----------

* ``opts``: Pointer to a :symbol:`mongoc_structured_log_opts_t` allocated with :symbol:`mongoc_structured_log_opts_new`, or NULL.

Description
-----------

This function releases all resources associated with a :symbol:`mongoc_structured_log_opts_t`.
Does nothing if ``opts`` is NULL.
