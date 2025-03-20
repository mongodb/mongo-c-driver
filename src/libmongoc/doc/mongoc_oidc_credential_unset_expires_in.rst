:man_page: mongoc_oidc_credential_unset_expires_in

mongoc_oidc_credential_unset_expires_in()
=========================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_credential_unset_expires_in (const mongoc_oidc_credential_t *cred);

Unset the expiry duration stored in the :symbol:`mongoc_oidc_credential_t` object.

.. important::

    A value of ``0`` is interpreted as immediate expiration.
    An unset value is interpreted as infinite expiry duration.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
  - :symbol:`mongoc_oidc_credential_get_expires_in`
  - :symbol:`mongoc_oidc_credential_set_expires_in`
