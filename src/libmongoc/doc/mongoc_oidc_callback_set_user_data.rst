:man_page: mongoc_oidc_callback_set_user_data

mongoc_oidc_callback_set_user_data()
====================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_callback_set_user_data (mongoc_oidc_callback_t *callback, void *user_data);

Store the provided pointer to user data.

.. warning::

    The lifetime of the object pointed to by ``user_data`` is managed the user, not by :symbol:`mongoc_oidc_callback_t`!

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t`.
* ``user_data``: a pointer to user data or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_get_user_data()`
