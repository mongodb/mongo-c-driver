:man_page: mongoc_kms_connect_callback_destroy

mongoc_kms_connect_callback_destroy()
=====================================

.. versionadded:: 2.4.0

Synopsis
--------

.. code-block:: c

  void
  mongoc_kms_connect_callback_destroy (mongoc_kms_connect_callback_t *callback);

Free all resources associated with a :symbol:`mongoc_kms_connect_callback_t` object. Does nothing if
``callback`` is ``NULL``.

Parameters
----------

* ``callback``: a :symbol:`mongoc_kms_connect_callback_t`. May be ``NULL``.

.. seealso::

  - :symbol:`mongoc_kms_connect_callback_t`
