:man_page: mongoc_oidc_credential_set_expires_in

mongoc_oidc_credential_set_expires_in()
=======================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_credential_set_expires_in (const mongoc_oidc_credential_t *cred, int64_t expires_in);

Store the expiry duration (in milliseconds) of the access token in the :symbol:`mongoc_oidc_credential_t` object.

The expiry duration is relative to the value returned by :symbol:`bson_get_monotonic_time()` immediately after the callback function has returned.

.. important::

    An unset value (default) or a value of ``0`` is interpreted as an infinite expiry duration.

Parameters
----------

* ``cred``: a :symbol:`mongoc_oidc_credential_t`.
* ``expires_in``: a non-negative integer.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
  - :symbol:`mongoc_oidc_credential_get_expires_in`
