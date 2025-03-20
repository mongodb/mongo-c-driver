:man_page: mongoc_oidc_callback_get_user_data

mongoc_oidc_callback_get_user_data()
====================================

Synopsis
--------

.. code-block:: c

  void *
  mongoc_oidc_callback_get_user_data (const mongoc_oidc_callback_t *callback);

Return the stored pointer to user data.

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t`.

Returns
-------

A pointer to user data or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_set_user_data()`
