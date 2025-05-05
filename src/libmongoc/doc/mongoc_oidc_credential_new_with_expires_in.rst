:man_page: mongoc_oidc_credential_new_with_expires_in

mongoc_oidc_credential_new_with_expires_in()
============================================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_credential_t *
  mongoc_oidc_credential_new_with_expires_in (const char *access_token, int64_t expires_in)

Create a new :symbol:`mongoc_oidc_credential_t` object which stores a copy of the provided OIDC access token and its expiry duration (in milliseconds).

The expiry duration will be evaluated relative to the value returned by :symbol:`bson_get_monotonic_time()` immediately after the callback function has returned.

To set an infinite expiry duration, use :symbol:`mongoc_oidc_credential_new()`.

.. warning::

    ``access_token`` is NOT directly validated by the driver.

Parameters
----------

* ``access_token``: an OIDC access token. Must not be ``NULL``.
* ``expires_in``: a non-negative integer.

Returns
-------

A new :symbol:`mongoc_oidc_credential_t` that must be freed with :symbol:`mongoc_oidc_credential_destroy()`, or ``NULL`` when an invalid argument was given.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
