:man_page: mongoc_oidc_callback_copy

mongoc_oidc_callback_copy()
===========================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_callback_t *
  mongoc_oidc_callback_copy (const mongoc_oidc_callback_t *callback)

Create a new :symbol:`mongoc_oidc_callback_t` object with the same callback function and user data pointer as an existing :symbol:`mongoc_oidc_callback_t`.

.. warning::

    The lifetime of the object pointed to by ``user_data`` is managed the user, not by :symbol:`mongoc_oidc_callback_t`!

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t` to copy. Must not be ``NULL``.

Returns
-------

A new :symbol:`mongoc_oidc_callback_t` that must be freed with :symbol:`mongoc_oidc_callback_destroy()`.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_new`
