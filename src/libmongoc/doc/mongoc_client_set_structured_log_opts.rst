:man_page: mongoc_client_set_structured_log_opts

mongoc_client_set_structured_log_opts()
=======================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_set_structured_log_opts (mongoc_client_t *client,
                                         const mongoc_structured_log_opts_t *opts);

Reconfigures this client's structured logging subsystem. See :doc:`structured_log`.

This function must not be called on clients that are part of a :symbol:`mongoc_client_pool_t`.
Structured logging for pools must be configured with :symbol:`mongoc_client_pool_set_structured_log_opts` before the first call to :symbol:`mongoc_client_pool_pop`.

The :symbol:`mongoc_structured_log_opts_t` is copied by the client and may be safely destroyed by the caller after this API call completes.
The application is responsible for ensuring any ``user_data`` referenced by ``opts`` remains valid for the lifetime of the client.

By default, the :symbol:`mongoc_client_t` will have log options captured from the environment during :symbol:`mongoc_client_new`.
See :symbol:`mongoc_structured_log_opts_new` for a list of the supported options.

The structured logging subsystem may be disabled by passing NULL as ``opts`` or equivalently by passing NULL as the :symbol:`mongoc_structured_log_func_t` in :symbol:`mongoc_structured_log_opts_set_handler`.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``opts``: A :symbol:`mongoc_structured_log_opts_t` allocated with :symbol:`mongoc_structured_log_opts_new`, or NULL to disable structured logging.

.. seealso::

  | :doc:`structured_log`
