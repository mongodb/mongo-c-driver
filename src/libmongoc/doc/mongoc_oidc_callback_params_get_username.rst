:man_page: mongoc_oidc_callback_params_get_username

mongoc_oidc_callback_params_get_username()
==========================================

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_oidc_callback_params_get_username (const mongoc_oidc_callback_params_t *params);

Return the username component of the URI of an associated :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

A string which must not be modified or freed, or ``NULL``.

Lifecycle
---------

The string is only valid for the duration of the invocation of the OIDC callback function.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_uri_t`
