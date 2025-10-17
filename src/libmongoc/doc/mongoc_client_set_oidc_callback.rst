:man_page: mongoc_client_set_oidc_callback

mongoc_client_set_oidc_callback()
=================================

Synopsis
--------

.. code-block:: c

   bool
   mongoc_client_set_oidc_callback(mongoc_client_t *client,
                                   const mongoc_oidc_callback_t *callback);

Register a callback for the ``MONGODB-OIDC`` authentication mechanism.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``callback``: A :symbol:`mongoc_oidc_callback_t`.

Returns
-------

Returns true on success. Returns false and logs on error.


.. seealso::
   | :doc:`mongoc_client_pool_set_oidc_callback` for setting a callback on a pooled client.
   | :doc:`mongoc_oidc_callback_t`
   | :doc:`mongoc_oidc_callback_params_t`
