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

* ``params``: A :symbol:`mongoc_oidc_callback_params_t` object representing in/out parameters of a :symbol:`mongoc_oidc_callback_t`.

Returns
-------

A :symbol:`mongoc_oidc_credential_t` object created with :symbol:`mongoc_oidc_credential_new()`, or ``NULL`` to indicate an error or timeout.

* The function MUST return a :symbol:`mongoc_oidc_credential_t` object to indicate success.
* The function MUST return ``NULL`` to indicate an error.
* The function MUST call :symbol:`mongoc_oidc_callback_params_cancel_with_timeout()` before returning ``NULL`` to indicate a timeout instead of an error.

The ``cancel_with_timeout`` out parameter is ignored if the return value is not ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_credential_t`
