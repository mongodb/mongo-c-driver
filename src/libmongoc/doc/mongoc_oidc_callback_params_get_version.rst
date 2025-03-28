:man_page: mongoc_oidc_callback_params_get_version

mongoc_oidc_callback_params_get_version()
=========================================

Synopsis
--------

.. code-block:: c

  int32_t
  mongoc_oidc_callback_params_get_version (const mongoc_oidc_callback_params_t *params);

Return the OIDC callback API version number.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

A positive integer.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
