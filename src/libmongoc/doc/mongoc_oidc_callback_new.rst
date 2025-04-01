:man_page: mongoc_oidc_callback_new

mongoc_oidc_callback_new()
==========================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_callback_t *
  mongoc_oidc_callback_new (mongoc_oidc_callback_fn_t fn)

Create a new :symbol:`mongoc_oidc_callback_t` object which stores the provided OIDC callback function.

Equivalent to calling :symbol:`mongoc_oidc_callback_new_with_user_data()` with ``user_data`` set to ``NULL``.

Parameters
----------

* ``fn``: a :symbol:`mongoc_oidc_callback_fn_t`. Must not be ``NULL``.

Returns
-------

A new :symbol:`mongoc_oidc_callback_t` that must be freed with :symbol:`mongoc_oidc_callback_destroy()`, or ``NULL`` when an invalid argument was given.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_new_with_user_data`
