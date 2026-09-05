:man_page: mongoc_kms_connect_callback_new_with_user_data

mongoc_kms_connect_callback_new_with_user_data()
================================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  mongoc_kms_connect_callback_t *
  mongoc_kms_connect_callback_new_with_user_data (mongoc_kms_connect_callback_fn_t fn, void *user_data);

Create a new :symbol:`mongoc_kms_connect_callback_t` object which stores the provided KMS connect callback
function and user data.

Parameters
----------

* ``fn``: a :symbol:`mongoc_kms_connect_callback_fn_t`. Must not be ``NULL``.
* ``user_data``: an arbitrary pointer passed unchanged to ``fn`` each time it is called. May be ``NULL``.

Returns
-------

A new :symbol:`mongoc_kms_connect_callback_t` that must be freed with
:symbol:`mongoc_kms_connect_callback_destroy()`, or ``NULL`` when an invalid argument was given.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_t`
  - :symbol:`mongoc_kms_connect_callback_new`
