:man_page: mongoc_kms_connect_callback_set_user_data

mongoc_kms_connect_callback_set_user_data()
===========================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  void
  mongoc_kms_connect_callback_set_user_data (mongoc_kms_connect_callback_t *callback, void *user_data);

Set the user data stored by ``callback``.

Parameters
----------

* ``callback``: a :symbol:`mongoc_kms_connect_callback_t`. Must not be ``NULL``.
* ``user_data``: an arbitrary pointer passed unchanged to the stored callback function each time it is
  called. May be ``NULL``.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_t`
  - :symbol:`mongoc_kms_connect_callback_get_user_data`
