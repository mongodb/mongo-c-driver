:man_page: mongoc_encrypt_text_opts_set_suffix

mongoc_encrypt_text_opts_set_suffix()
=====================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_opts_set_suffix(
      mongoc_encrypt_text_opts_t *opts,
      mongoc_encrypt_text_suffix_opts_t *sopts);

.. versionadded:: 1.26.0

Sets the suffix options for text search encryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_opts_t`.
* ``sopts``: A :symbol:`mongoc_encrypt_text_suffix_opts_t` to set as suffix options.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_new`
   | :symbol:`mongoc_encrypt_text_suffix_opts_new`
