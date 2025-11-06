:man_page: mongoc_client_pool_set_oidc_callback

mongoc_client_pool_set_oidc_callback()
======================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_client_pool_set_oidc_callback(mongoc_client_pool_t *pool,
                                        const mongoc_oidc_callback_t *callback);

Register a callback for the ``MONGODB-OIDC`` authentication mechanism.

Parameters
----------

* ``pool``: A :symbol:`mongoc_client_pool_t`.
* ``callback``: A :symbol:`mongoc_oidc_callback_t`.

Returns
-------

Returns true on success. Returns false and logs on error.

.. include:: includes/mongoc_client_pool_call_once.txt

.. seealso::
   | :doc:`mongoc_client_set_oidc_callback` for setting a callback on a single-threaded client.
   | :doc:`mongoc_oidc_callback_t`
   | :doc:`mongoc_oidc_callback_params_t`
