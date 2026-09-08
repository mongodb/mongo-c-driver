:man_page: mongoc_kms_connect_callback_params_get_error

mongoc_kms_connect_callback_params_get_error()
==============================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  bson_error_t *
  mongoc_kms_connect_callback_params_get_error (mongoc_kms_connect_callback_params_t *params);

Return the :symbol:`bson_error_t` the callback function must set when it fails to establish a connection.

Parameters
----------

* ``params``: a :symbol:`mongoc_kms_connect_callback_params_t`. Must not be ``NULL``.

Returns
-------

A writable :symbol:`bson_error_t`. Only valid for the duration of the callback function's execution. The
error is cleared before the callback function is invoked. A :symbol:`mongoc_kms_connect_callback_fn_t` that
returns ``NULL`` is expected to set a descriptive error message and domain/code here; if it does not, the
driver reports a generic connection failure instead.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_kms_connect_callback_fn_t`
