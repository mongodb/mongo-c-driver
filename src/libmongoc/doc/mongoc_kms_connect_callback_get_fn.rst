:man_page: mongoc_kms_connect_callback_get_fn

mongoc_kms_connect_callback_get_fn()
====================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  mongoc_kms_connect_callback_fn_t
  mongoc_kms_connect_callback_get_fn (const mongoc_kms_connect_callback_t *callback);

Return the :symbol:`mongoc_kms_connect_callback_fn_t` stored by ``callback``.

Parameters
----------

* ``callback``: a :symbol:`mongoc_kms_connect_callback_t`. Must not be ``NULL``.

Returns
-------

The stored :symbol:`mongoc_kms_connect_callback_fn_t`.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_t`
