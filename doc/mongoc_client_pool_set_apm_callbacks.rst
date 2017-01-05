:man_page: mongoc_client_pool_set_apm_callbacks

mongoc_client_pool_set_apm_callbacks()
======================================

Synopsis
--------

.. code-block:: none

  bool
  mongoc_client_pool_set_apm_callbacks (mongoc_client_pool_t   *pool,
                                        mongoc_apm_callbacks_t *callbacks,
                                        void                   *context);

Register a set of callbacks to receive Application Performance Monitoring events.

This function can only be called once on a pool, and must be called before the first :symbol:`mongoc_client_pool_pop <mongoc_client_pool_pop>`.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t <mongoc_client_pool_t>`.
* ``callbacks``: A :symbol:`mongoc_apm_callbacks_t <mongoc_apm_callbacks_t>`.
* ``context``: Optional pointer to include with each event notification.

Returns
-------

Returns true on success, otherwise false and an error is logged.

See Also
--------

:doc:`Introduction to Application Performance Monitoring <application-performance-monitoring>`

