:man_page: mongoc_client_encryption_opts_set_key_expiration

mongoc_client_encryption_opts_set_key_expiration()
========================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_opts_set_key_expiration (
      mongoc_client_encryption_opts_t *opts, uint64_t cache_expiration_ms);


Parameters
----------

* ``opts``: The :symbol:`mongoc_client_encryption_opts_t`
* ``cache_expiration_ms``: The data encryption key cache expiration time in milliseconds. Defaults to 60,000. 0 means "never expire".

.. seealso::

  | :symbol:`mongoc_client_encryption_new()`

  | `In-Use Encryption <in-use-encryption_>`_

