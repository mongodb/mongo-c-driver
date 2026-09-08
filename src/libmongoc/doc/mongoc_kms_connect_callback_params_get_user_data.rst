:man_page: mongoc_kms_connect_callback_params_get_user_data

mongoc_kms_connect_callback_params_get_user_data()
==================================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  void *
  mongoc_kms_connect_callback_params_get_user_data (const mongoc_kms_connect_callback_params_t *params);

Return the user data stored by the :symbol:`mongoc_kms_connect_callback_t` being invoked.

Parameters
----------

* ``params``: a :symbol:`mongoc_kms_connect_callback_params_t`. Must not be ``NULL``.

Returns
-------

The stored user data, which may be ``NULL``.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_params_t`
  - :symbol:`mongoc_kms_connect_callback_set_user_data`
