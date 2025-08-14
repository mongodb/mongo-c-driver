:man_page: mongoc_oidc_callback_new_with_user_data

mongoc_oidc_callback_new_with_user_data()
=========================================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_callback_t *
  mongoc_oidc_callback_new_with_user_data (mongoc_oidc_callback_fn_t fn, void *user_data)

Create a new :symbol:`mongoc_oidc_callback_t` object which stores the provided OIDC callback function and pointer to user data.

.. warning::

    The lifetime of the object pointed to by ``user_data`` is managed the user, not by :symbol:`mongoc_oidc_callback_t`!

Parameters
----------

* ``fn``: a :symbol:`mongoc_oidc_callback_fn_t`. Must not be ``NULL``.
* ``user_data``: a pointer to user data or ``NULL``.

Returns
-------

A new :symbol:`mongoc_oidc_callback_t` that must be freed with :symbol:`mongoc_oidc_callback_destroy()`, or ``NULL`` when an invalid argument was given.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_new`
