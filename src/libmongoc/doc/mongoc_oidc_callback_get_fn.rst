:man_page: mongoc_oidc_callback_get_fn

mongoc_oidc_callback_get_fn()
=============================

Synopsis
--------

.. code-block:: c

  mongoc_oidc_callback_fn_t
  mongoc_oidc_callback_get_fn (const mongoc_oidc_callback_t *callback);

Return the stored pointer to the callback function.

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t`.

Returns
-------

A :symbol:`mongoc_oidc_callback_fn_t` or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
