:man_page: mongoc_oidc_credential_t

mongoc_oidc_credential_t
========================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_oidc_credential_t mongoc_oidc_credential_t;

Represents the return value of a :symbol:`mongoc_oidc_callback_fn_t`.

The value will be returned by the :symbol:`mongoc_oidc_callback_fn_t` stored in an :symbol:`mongoc_oidc_callback_t` object when it is invoked by an associated :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_oidc_credential_new
    mongoc_oidc_credential_new_with_expires_in
    mongoc_oidc_credential_destroy
    mongoc_oidc_credential_get_access_token
    mongoc_oidc_credential_get_expires_in

Return Values
-------------

The list of currently supported return values are:

.. list-table::
    :widths: auto

    * - Value
      - Versions
      - Description
    * - ``access_token``
      - 1
      - The OIDC access token.
    * - ``expires_in``
      - 1
      - An optional expiration duration (in milliseconds).

The "Version" column indicates the OIDC callback API versions for which the parameter is applicable.

Access Token
````````````

An OIDC access token (a signed JWT token).

.. warning::

    ``access_token`` is NOT directly validated by the driver.

Expiry Duration
```````````````

An optional expiry duration (in milliseconds) for the access token.

.. important::

    An unset value is interpreted as an infinite expiry duration.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
