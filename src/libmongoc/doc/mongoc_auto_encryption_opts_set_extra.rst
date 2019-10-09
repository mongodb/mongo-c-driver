:man_page: mongoc_auto_encryption_opts_set_extra

mongoc_auto_encryption_opts_set_extra()
=======================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_extra (mongoc_auto_encryption_opts_t *opts,
                                          const bson_t *extra);


Parameters
----------

* ``opts``: The :symbol:`mongoc_auto_encryption_opts_t`
* ``extra``: A :symbol:`bson_t` of additional options.

See also
--------

* :symbol:`mongoc_client_enable_auto_encryption()`
* The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`
