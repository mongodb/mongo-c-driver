:man_page: mongoc_oidc_credential_set_access_token

mongoc_oidc_credential_set_access_token()
=========================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_credential_set_access_token (const mongoc_oidc_credential_t *cred, const char *access_token);

Store a copy of the provided OIDC access token in the :symbol:`mongoc_oidc_credential_t` object.

.. warning::

    ``access_token`` is NOT directly validated by the driver.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.
* ``access_token``: a string or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
  - :symbol:`mongoc_oidc_credential_get_access_token`
