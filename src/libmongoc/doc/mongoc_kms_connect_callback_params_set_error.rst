:man_page: mongoc_kms_connect_callback_params_set_error

mongoc_kms_connect_callback_params_set_error()
===============================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  mongoc_stream_t *
  mongoc_kms_connect_callback_params_set_error (mongoc_kms_connect_callback_params_t *params, const char *msg);

Called by a :symbol:`mongoc_kms_connect_callback_fn_t` to report failure to connect.

Parameters
----------

* ``params``: a :symbol:`mongoc_kms_connect_callback_params_t`. Must not be ``NULL``.
* ``msg``: an optional descriptive error message. May be ``NULL``.

Returns
-------

``NULL``, as a convenience for returning directly from the callback function:

.. code-block:: c

  if (can_fail()) {
     return mongoc_kms_connect_callback_params_set_error (params, "callback failed to call can_fail()");
  }

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_kms_connect_callback_fn_t`
