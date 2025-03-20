:man_page: mongoc_oidc_credential_destroy

mongoc_oidc_credential_destroy()
================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_credential_destroy (mongoc_oidc_credential_t *credential)

Release all resources associated with the given :symbol:`mongoc_oidc_credential_t` object.

Parameters
----------

* ``credential``: a :symbol:`mongoc_oidc_credential_t` or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_credential_t`
