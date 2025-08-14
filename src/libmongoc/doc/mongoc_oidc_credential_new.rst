:man_page: mongoc_oidc_credential_new

mongoc_oidc_credential_new()
============================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_credential_t *
  mongoc_oidc_credential_new (const char *access_token)

Create a new :symbol:`mongoc_oidc_credential_t` object which stores a copy of the provided OIDC access token with an infinite expiry duration.

To set a finite expiry duration, use :symbol:`mongoc_oidc_credential_new_with_expires_in()`.

.. warning::

    ``access_token`` is NOT directly validated by the driver.

Parameters
----------

* ``access_token``: an OIDC access token. Must not be ``NULL``.

Returns
-------

A new :symbol:`mongoc_oidc_credential_t` that must be freed with :symbol:`mongoc_oidc_credential_destroy()`, or ``NULL`` when an invalid argument was given.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
