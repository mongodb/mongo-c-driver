:man_page: mongoc_encrypt_text_opts_set_substring

mongoc_encrypt_text_opts_set_substring()
========================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_opts_set_substring(
      mongoc_encrypt_text_opts_t *opts,
      mongoc_encrypt_text_substring_opts_t *ssopts);

.. versionadded:: 1.26.0

Sets the substring options for text search encryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_opts_t`.
* ``ssopts``: A :symbol:`mongoc_encrypt_text_substring_opts_t` to set as substring options.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_new`
   | :symbol:`mongoc_encrypt_text_substring_opts_new`
