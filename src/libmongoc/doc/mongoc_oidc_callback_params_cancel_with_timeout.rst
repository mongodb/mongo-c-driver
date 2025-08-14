:man_page: mongoc_oidc_callback_params_cancel_with_timeout

mongoc_oidc_callback_params_cancel_with_timeout()
=================================================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_credential_t *
  mongoc_oidc_callback_params_cancel_with_timeout (mongoc_oidc_callback_params_t *params);

Set the out parameter indicating cancellation of the callback function due to a timeout instead of an error.

.. note::

  If the callback function returns a not-null value, the value of this out parameter is ignored.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
