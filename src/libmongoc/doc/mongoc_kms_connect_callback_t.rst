:man_page: mongoc_kms_connect_callback_t

mongoc_kms_connect_callback_t
=============================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_kms_connect_callback_t mongoc_kms_connect_callback_t;

:symbol:`mongoc_kms_connect_callback_t` stores a user-defined callback function
:symbol:`mongoc_kms_connect_callback_fn_t` that opens a transport connection to a KMS endpoint, along with
optional user data passed to that function.

This is the primary extension point for routing KMS traffic through an HTTP ``CONNECT`` proxy.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_kms_connect_callback_new
    mongoc_kms_connect_callback_new_with_user_data
    mongoc_kms_connect_callback_destroy
    mongoc_kms_connect_callback_get_fn
    mongoc_kms_connect_callback_get_user_data
    mongoc_kms_connect_callback_set_user_data

Lifecycle
---------

:symbol:`mongoc_client_encryption_opts_set_kms_connect_callback` and
:symbol:`mongoc_auto_encryption_opts_set_kms_connect_callback` store a copy of the callback object. The
callback object may be destroyed immediately after being set. The function and optional user data stored by
:symbol:`mongoc_kms_connect_callback_t` must outlive any associated client, client pool, or
:symbol:`mongoc_client_encryption_t` object which may invoke the stored callback function.

Thread Safety
-------------

The stored callback function may be invoked by more than one thread at a time when the associated options are
used with a :symbol:`mongoc_client_pool_t`.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_fn_t`
  - :symbol:`mongoc_client_encryption_opts_set_kms_connect_callback`
  - :symbol:`mongoc_auto_encryption_opts_set_kms_connect_callback`
