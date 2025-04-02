:man_page: mongoc_oidc_callback_params_t

mongoc_oidc_callback_params_t
=============================

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_oidc_callback_params_t mongoc_oidc_callback_params_t;

Represents the in/out parameters of a :symbol:`mongoc_oidc_callback_t`.

The parameters will be passed to the :symbol:`mongoc_oidc_callback_fn_t` stored in an :symbol:`mongoc_oidc_callback_t` object when it is invoked by an :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_oidc_callback_params_get_version
    mongoc_oidc_callback_params_get_user_data
    mongoc_oidc_callback_params_get_timeout
    mongoc_oidc_callback_params_get_username
    mongoc_oidc_callback_params_cancel_with_timeout

Parameters
----------

The list of currently supported parameters are:

.. list-table::
    :widths: auto

    * - Parameter
      - Versions
      - Description
    * - ``version``
      - 1
      - The current OIDC callback API version number.
    * - ``user_data``
      - 1
      - A pointer to data provided by the user.
    * - ``timeout``
      - 1
      - The timestamp after which the callback function must report a timeout.
    * - ``username``
      - 1
      - The username specified by the URI of the associated client object.
    * - ``cancel_with_timeout``
      - 1
      - An out parameter indicating cancellation of the callback function due to a timeout instead of an error.

The "Version" column indicates the OIDC callback API versions for which the parameter is applicable.

Version
```````

The ``version`` parameter is used to communicate backward compatible changes to the OIDC callback API (i.e. the addition of a new parameter).

This parameter may be used to detect when existing usage of :symbol:`mongoc_oidc_callback_t` or a relevant callback function may need to be reviewed.

For example, users may add the following check to their callback function:

.. code-block:: c

    mongoc_oidc_credential_t *
    example_callback_fn (mongoc_oidc_callback_params_t *params)
    {
       // A runtime message that new features are available in the OIDC Callback API.
       if (mongoc_oidc_callback_params_get_version (params) > 1) {
          printf ("OIDC Callback API has been updated to a new version!");
       }

       // ...
    }

User Data
`````````

The ``user_data`` parameter may be used to pass additional arguments to the callback function or to return additional values out of the callback function.

This parameter must be set in advance via :symbol:`mongoc_oidc_callback_set_user_data()` before the :symbol:`mongoc_oidc_callback_t` object is associated with a :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object.

.. warning::

    The lifetime of the object pointed to by ``user_data`` is managed the user, not by :symbol:`mongoc_oidc_callback_t`!

.. code-block:: c

    typedef struct {
       int counter;
       const char *error_message;
    } user_data_t;

    mongoc_oidc_credential_t *
    example_callback_fn (mongoc_oidc_callback_params_t *params)
    {
       user_data_t *user_data = (user_data_t *) mongoc_oidc_callback_params_get_user_data (params);

       user_data->counter += 1;

       // ...

       if (/* ... */) {
          user_data->error_message = "OIDC callback failed due to ...";
          return NULL;
       }

       // ...
    }

    void
    example (void)
    {
       mongoc_client_t *client = /* ... */;
       bson_error_t error;

       {
          user_data_t *user_data = malloc (sizeof (*user_data));
          *user_data = (user_data_t){.counter = 0, .error_message = NULL};
          mongoc_oidc_callback_t *callback = mongoc_oidc_callback_new (&example_callback_fn, (void *) user_data);
          mongoc_client_set_oidc_callback (client, callback);
          mongoc_oidc_callback_destroy (callback);
       }

       // ... client operations ...

       {
          const mongoc_oidc_callback_t *callback = mongoc_client_get_oidc_callback (client);
          user_data_t *user_data = (user_data_t *) mongoc_oidc_callback_get_user_data (callback);

          if (error.code != 0) {
             printf ("client error message: %s\n", error.message);
          }

          if (user_data->error_message) {
             printf ("custom error message: %s\n", user_data->error_message);
          }

          printf ("The callback function was invoked %d times!", user_data->counter);

          free (user_data);
       }

       mongoc_client_destroy (client);
    }

Timeout
```````

The ``timeout`` parameter is used to determine when the callback function should report cancellation due to a timeout.

When :symbol:`bson_get_monotonic_time()` is greater than ``timeout``, the callback function must invoke :symbol:`mongoc_oidc_callback_params_cancel_with_timeout()` and return ``NULL``.

Username
````````

The ``username`` parameter is the value of the username component of the URI of the associated :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t` object from which the callback function is invoked.

Cancel With Timeout
```````````````````

The ``cancel_with_timeout`` out parameter indicates cancellation of the callback function due to a timeout instead of an error.

.. important::

    The callback function MUST return ``NULL``, otherwise the invocation will be interpreted as a success even when ``cancel_with_timeout`` is set.

.. code-block:: c

    mongoc_oidc_credential_t *
    example_callback_fn (mongoc_oidc_callback_params_t *params) {
       const int64_t *timeout = mongoc_oidc_callback_params_get_timeout (params);

       // NULL means "infinite" timeout.
       if (timeout && bson_get_monotonic_time () > *timeout) {
          return mongoc_oidc_callback_params_cancel_with_timeout (params);
       }

       // ...
    }

Error Handling
``````````````

A ``NULL`` return value (without setting ``cancel_with_timeout``) indicates failure to provide an access token due to an error.

.. important::

    The callback function MUST return ``NULL``, otherwise the invocation will be interpreted as a success.

.. code-block:: c

    mongoc_oidc_credential_t *
    example_callback_fn (mongoc_oidc_callback_params_t *params) {
       // ...

       if (/* ... */) {
          // The OIDC callback function could not provide an access token due to an error.
          return NULL;
       }

       // ...
    }

.. seealso::

  - :symbol:`mongoc_oidc_callback_t`
  - :symbol:`mongoc_oidc_callback_fn_t`
