:man_page: mongoc_oidc_callback_t

mongoc_oidc_callback_t
======================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_oidc_callback_t mongoc_oidc_callback_t;

:symbol:`mongoc_oidc_callback_t` represents a user-defined callback function :symbol:`mongoc_oidc_callback_fn_t` that returns an OIDC access token.

The callback may be used to integrate with OIDC providers that are not supported by the built-in provider integrations (`Authentication Mechanism Properties <_authentication_mechanism_properties>`_).

Lifecycle
---------

The function and optional user data stored by :symbol:`mongoc_oidc_callback_t` must outlive any associated client or client pool object which may invoke the stored callback function.

Thread Safety
-------------

The callback function stored by a :symbol:`mongoc_oidc_callback_t` object will be invoked by at most one thread at a time for an associated :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object:

.. code-block:: c

    static mongoc_oidc_credential_t *
    single_thread_only (mongoc_oidc_callback_params_t *params)
    {
       // This function does not need to support invocation by more than thread at a time.

       // ...
    }

    void
    with_single_client (void)
    {
       mongoc_client_t *client = /* ... */;

       {
          mongoc_oidc_callback_t *callback = mongoc_oidc_callback_new ();
          mongoc_oidc_callback_set_fn (callback, &single_thread_only); // OK
          mongoc_client_set_oidc_callback (client, callback);
          mongoc_oidc_callback_destroy (callback);
       }

       // ... client operations ...

       mongoc_client_destroy (client);
    }

    void
    with_single_pool (void)
    {
       mongoc_client_pool_t *pool = /* ... */;

       {
          mongoc_oidc_callback_t *callback = mongoc_oidc_callback_new ();
          mongoc_oidc_callback_set_fn (callback, &single_thread_only); // OK
          mongoc_client_pool_set_oidc_callback (pool, callback);
          mongoc_oidc_callback_destroy (callback);
       }

       // ... client pool operations ...

       mongoc_client_pool_destroy (pool);
    }

If the callback is associated with more than one :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object, the callback function MUST support invocation by more than one thread at a time:

.. code-block:: c

    static mongoc_oidc_credential_t *
    many_threads_possible (mongoc_oidc_callback_params_t *params)
    {
       // This function MUST support invocation by more than one thread at a time.

       // ...
    }

    void
    with_many_clients (void)
    {
       mongoc_client_t *client_a = /* ... */;
       mongoc_client_t *client_b = /* ... */;

       {
          mongoc_oidc_callback_t *callback = mongoc_oidc_callback_new ();
          mongoc_oidc_callback_set_fn (callback, &many_threads_possible);
          mongoc_client_set_oidc_callback (client_a, callback);
          mongoc_client_set_oidc_callback (client_b, callback);
          mongoc_oidc_callback_destroy (callback);
       }

       // ... client operations ...

       mongoc_client_destroy (client_a);
       mongoc_client_destroy (client_b);
    }

    void
    with_many_pools (void)
    {
       mongoc_client_pool_t *pool_a = /* ... */;
       mongoc_client_pool_t *pool_b = /* ... */;

       {
          mongoc_oidc_callback_t *callback = mongoc_oidc_callback_new ();
          mongoc_oidc_callback_set_fn (callback, &many_threads_possible);
          mongoc_client_pool_set_oidc_callback (pool_a, callback);
          mongoc_client_pool_set_oidc_callback (pool_b, callback);
          mongoc_oidc_callback_destroy (callback);
       }

       // ... client pool operations ...

       mongoc_client_pool_destroy (pool_a);
       mongoc_client_pool_destroy (pool_b);
    }

.. seealso::

  - :symbol:`mongoc_client_t`
  - :symbol:`mongoc_client_pool_t`

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_oidc_callback_fn_t
    mongoc_oidc_callback_new
    mongoc_oidc_callback_destroy
    mongoc_oidc_callback_get_fn
    mongoc_oidc_callback_set_fn
    mongoc_oidc_callback_get_user_data
    mongoc_oidc_callback_set_user_data
    mongoc_oidc_callback_params_t
    mongoc_oidc_callback_params_get_version
    mongoc_oidc_callback_params_get_user_data
    mongoc_oidc_callback_params_get_timeout
    mongoc_oidc_callback_params_get_username
    mongoc_oidc_callback_params_cancel_with_timeout
    mongoc_oidc_credential_t
    mongoc_oidc_credential_new
    mongoc_oidc_credential_destroy
    mongoc_oidc_credential_get_access_token
    mongoc_oidc_credential_set_access_token
    mongoc_oidc_credential_get_expires_in
    mongoc_oidc_credential_set_expires_in
