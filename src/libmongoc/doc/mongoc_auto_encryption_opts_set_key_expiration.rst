:man_page: mongoc_auto_encryption_opts_set_key_expiration

mongoc_auto_encryption_opts_set_key_expiration()
========================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_key_expiration (
      mongoc_auto_encryption_opts_t *opts, uint64_t cache_expiration_ms);


Parameters
----------

* ``opts``: The :symbol:`mongoc_auto_encryption_opts_t`
* ``cache_expiration_ms``: The data encryption key cache expiration time in milliseconds.

.. seealso::

  | :symbol:`mongoc_client_enable_auto_encryption()`

  | `In-Use Encryption <in-use-encryption_>`_

