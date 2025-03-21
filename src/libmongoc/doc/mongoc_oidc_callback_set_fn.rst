:man_page: mongoc_oidc_callback_set_fn

mongoc_oidc_callback_set_fn()
=============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_callback_set_fn (const mongoc_oidc_callback_t *callback, mongoc_oidc_callback_fn_t fn);

Store the provided pointer to the callback function.

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t`.
* ``fn``: a :symbol:`mongoc_oidc_callback_fn_t`

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
  - :symbol:`mongoc_oidc_callback_get_fn`
