:man_page: mongoc_client_pool_set_structured_log_opts

mongoc_client_pool_set_structured_log_opts()
============================================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_client_pool_set_structured_log_opts (mongoc_client_pool_t *pool,
                                              const mongoc_structured_log_opts_t *opts);

Reconfigures this client pool's structured logging subsystem. See :doc:`structured_log`.

The :symbol:`mongoc_structured_log_opts_t` is copied by the pool and may be safely destroyed by the caller after this API call completes.
The application is responsible for ensuring any ``user_data`` referenced by ``opts`` remains valid for the lifetime of the pool.

By default, the :symbol:`mongoc_client_pool_t` will have log options captured from the environment during :symbol:`mongoc_client_pool_new`.
See :symbol:`mongoc_structured_log_opts_new` for a list of the supported options.

The structured logging subsystem may be disabled by passing NULL as ``opts`` or equivalently by passing NULL as the :symbol:`mongoc_structured_log_func_t` in :symbol:`mongoc_structured_log_opts_set_handler`.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``opts``: A :symbol:`mongoc_structured_log_opts_t` allocated with :symbol:`mongoc_structured_log_opts_new`, or NULL to disable structured logging.

Returns
-------

Returns true when used correctly. If called multiple times per pool or after the first client is initialized, returns false and logs a warning.

.. include:: includes/mongoc_client_pool_call_once.txt

Thread safety within the handler is the application's responsibility. Handlers may be invoked concurrently by multiple pool users.

.. seealso::

  | :doc:`structured_log`
