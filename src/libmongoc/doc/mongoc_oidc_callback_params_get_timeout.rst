:man_page: mongoc_oidc_callback_params_get_timeout

mongoc_oidc_callback_params_get_timeout()
=========================================

Synopsis
--------

.. code-block:: c

  int64_t
  mongoc_oidc_callback_params_get_timeout (const mongoc_oidc_callback_params_t *params);

Return the :symbol:`bson_get_monotonic_time()` value used to determine when a timeout must occur.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

A :symbol:`bson_get_monotonic_time()` value or ``NULL``.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
