:man_page: mongoc_oidc_credential_get_expires_in

mongoc_oidc_credential_get_expires_in()
=======================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_oidc_credential_get_expires_in (const mongoc_oidc_credential_t *cred);

Return the expiry duration (in milliseconds) for the access token stored in :symbol:`mongoc_oidc_credential_t`.

.. important::

    A value of ``0`` is interpreted as an infinite expiry duration.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.

Returns
-------

The expiry duration (in milliseconds).

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_credential_new_with_expires_in`
  - :symbol:`mongoc_oidc_callback_fn_t`
