:man_page: mongoc_oidc_callback_destroy

mongoc_oidc_callback_destroy()
==============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_oidc_callback_destroy (mongoc_oidc_callback_t *callback)

Release all resources associated with the given :symbol:`mongoc_oidc_callback_t` object.

.. warning::

    The lifetime of the object pointed to by ``user_data`` is managed the user, not by :symbol:`mongoc_oidc_callback_t`!

Parameters
----------

* ``callback``: a :symbol:`mongoc_oidc_callback_t` or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
