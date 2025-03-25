:man_page: mongoc_oidc_callback_fn_t

mongoc_oidc_callback_fn_t
=========================

Synopsis
--------

.. code-block:: c

  typedef mongoc_oidc_credential_t *(*mongoc_oidc_callback_fn_t) (mongoc_oidc_callback_params_t *params);

The type of the function pointer stored by :symbol:`mongoc_oidc_callback_t`.

Parameters
----------

* ``params``: A :symbol:`mongoc_oidc_callback_params_t` object.

Returns
-------

A :symbol:`mongoc_oidc_credential_t` object created with :symbol:`mongoc_oidc_credential_new()`, or ``NULL`` to indicate cancellation.

* The function MUST return a :symbol:`mongoc_oidc_credential_t` object to indicate success.
* The function MUST call :symbol:`mongoc_oidc_callback_params_cancel_with_timeout()` and return ``NULL`` to indicate a timeout.
* The function MUST call :symbol:`mongoc_oidc_callback_params_cancel_with_error()` and return ``NULL`` to indicate an error.

The ``cancel_with_timeout`` and ``cancel_with_error`` out parameters are ignored if the return value is not ``NULL``.

A ``NULL`` return value without setting either ``cancel_with_timeout`` or ``cancel_with_error`` will be interpreted as a client error.

The ``cancel_with_timeout`` out parameter is ignored when ``cancel_with_error`` is set.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_credential_t`
