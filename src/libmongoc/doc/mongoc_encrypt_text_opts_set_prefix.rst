:man_page: mongoc_encrypt_text_opts_set_prefix

mongoc_encrypt_text_opts_set_prefix()
=====================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_opts_set_prefix(
      mongoc_encrypt_text_opts_t *opts,
      mongoc_encrypt_text_prefix_opts_t *popts);

.. versionadded:: 1.26.0

Sets the prefix options for text search encryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_opts_t`.
* ``popts``: A :symbol:`mongoc_encrypt_text_prefix_opts_t` to set as prefix options.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_new`
   | :symbol:`mongoc_encrypt_text_prefix_opts_new`
