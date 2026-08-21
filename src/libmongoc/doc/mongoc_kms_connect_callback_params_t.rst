:man_page: mongoc_kms_connect_callback_params_t

mongoc_kms_connect_callback_params_t
====================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  typedef struct _mongoc_kms_connect_callback_params_t mongoc_kms_connect_callback_params_t;

:symbol:`mongoc_kms_connect_callback_params_t` describes the KMS endpoint that a
:symbol:`mongoc_kms_connect_callback_fn_t` is being asked to connect to. It is the only parameter passed to
the callback function.

New accessors may be added in the future to pass additional information to the callback function.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_kms_connect_callback_params_get_host
    mongoc_kms_connect_callback_params_get_port
    mongoc_kms_connect_callback_params_get_user_data
    mongoc_kms_connect_callback_params_get_error

Lifecycle
---------

The driver creates and destroys :symbol:`mongoc_kms_connect_callback_params_t`. A
:symbol:`mongoc_kms_connect_callback_params_t` is only valid for the duration of the call to the
:symbol:`mongoc_kms_connect_callback_fn_t` it is passed to. Do not retain it, or anything it returns, beyond
the callback function's return.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_fn_t`
  - :symbol:`mongoc_kms_connect_callback_t`
