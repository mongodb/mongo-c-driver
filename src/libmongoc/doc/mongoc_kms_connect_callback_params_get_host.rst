:man_page: mongoc_kms_connect_callback_params_get_host

mongoc_kms_connect_callback_params_get_host()
=============================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  const char *
  mongoc_kms_connect_callback_params_get_host (const mongoc_kms_connect_callback_params_t *params);

Return the hostname of the KMS endpoint the driver needs to reach (e.g. ``"kms.us-east-1.amazonaws.com"``).

Parameters
----------

* ``params``: a :symbol:`mongoc_kms_connect_callback_params_t`. Must not be ``NULL``.

Returns
-------

The KMS hostname. Only valid for the duration of the callback function's execution.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_kms_connect_callback_params_get_port`
