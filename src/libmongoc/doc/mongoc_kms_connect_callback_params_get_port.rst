:man_page: mongoc_kms_connect_callback_params_get_port

mongoc_kms_connect_callback_params_get_port()
=============================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  uint16_t
  mongoc_kms_connect_callback_params_get_port (const mongoc_kms_connect_callback_params_t *params);

Return the port number of the KMS endpoint the driver needs to reach (typically ``443``).

Parameters
----------

* ``params``: a :symbol:`mongoc_kms_connect_callback_params_t`. Must not be ``NULL``.

Returns
-------

The KMS port number.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_kms_connect_callback_params_get_host`
