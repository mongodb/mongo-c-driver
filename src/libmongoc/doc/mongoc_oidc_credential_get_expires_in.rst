:man_page: mongoc_oidc_credential_get_expires_in

mongoc_oidc_credential_get_expires_in()
=======================================

Synopsis
--------

.. code-block:: c

  const int64_t *
  mongoc_oidc_credential_get_expires_in (const mongoc_oidc_credential_t *cred);

Return the optional expiry duration (in milliseconds) for the access token stored in :symbol:`mongoc_oidc_credential_t`.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.

Returns
-------

The expiry duration (in milliseconds), or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_credential_new_with_expires_in`
  - :symbol:`mongoc_oidc_callback_fn_t`
