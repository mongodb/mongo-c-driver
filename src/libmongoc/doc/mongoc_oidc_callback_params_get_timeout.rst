:man_page: mongoc_oidc_callback_params_get_timeout

mongoc_oidc_callback_params_get_timeout()
=========================================

Synopsis
--------

.. code-block:: c

  const int64_t *
  mongoc_oidc_callback_params_get_timeout (const mongoc_oidc_callback_params_t *params);

Return a value comparable with :symbol:`bson_get_monotonic_time()` to determine when a timeout must occur.

A ``NULL`` (unset) return value means "infinite" timeout.

Parameters
----------

* ``params``: a :symbol:`mongoc_oidc_callback_params_t`.

Returns
-------

A value comparable with :symbol:`bson_get_monotonic_time()`, or ``NULL``.

Lifecycle
---------

The pointed-to ``int64_t`` is only valid for the duration of the invocation of the OIDC callback function.

.. seealso::

  - :symbol:`mongoc_oidc_callback_params_t`
  - :symbol:`mongoc_oidc_callback_t`
