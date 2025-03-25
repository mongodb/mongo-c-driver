:man_page: mongoc_oidc_credential_get_access_token

mongoc_oidc_credential_get_access_token()
=========================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_oidc_credential_get_access_token (const mongoc_oidc_credential_t *cred);

Return the access token stored in the :symbol:`mongoc_oidc_credential_t` object.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.

Returns
-------

A string which must not be modified or freed.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
  - :symbol:`mongoc_oidc_credential_new`
  - :symbol:`mongoc_oidc_credential_new_with_expires_in`
