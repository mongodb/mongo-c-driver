:man_page: mongoc_oidc_callback_params_get_user_data

mongoc_oidc_callback_params_get_user_data()
===========================================

Synopsis
--------

.. code-block:: c

  void *
  mongoc_oidc_callback_params_get_user_data (const mongoc_oidc_callback_params_t *params);

Return the pointer to user data which was stored by an associated :symbol:`mongoc_oidc_callback_t` object.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

A pointer to user data or ``NULL``.

Lifecycle
---------

The lifetime of the object pointed to by ``user_data`` is managed the user.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_set_user_data()`
