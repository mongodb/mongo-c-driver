:man_page: mongoc_oidc_callback_params_get_timeout

mongoc_oidc_callback_params_get_timeout()
=========================================

Synopsis
--------

.. code-block:: c

  const int64_t *
  mongoc_oidc_callback_params_get_timeout (const mongoc_oidc_callback_params_t *params);

Return a value comparable with :symbol:`bson_get_monotonic_time()` to determine when a timeout must occur.

The return value is an absolute time point, not a duration. A callback can signal a timeout error using
:symbol:`mongoc_oidc_callback_params_cancel_with_timeout`. Example:

.. code-block:: c

    mongoc_oidc_credential_t *
    example_callback_fn (mongoc_oidc_callback_params_t *params) {
       const int64_t *timeout = mongoc_oidc_callback_params_get_timeout (params);

       // NULL means "infinite" timeout.
       if (timeout && bson_get_monotonic_time () > *timeout) {
          return mongoc_oidc_callback_params_cancel_with_timeout (params);
       }

       // ... your code here ...
    }

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
  - :symbol:`mongoc_oidc_callback_params_cancel_with_timeout`
