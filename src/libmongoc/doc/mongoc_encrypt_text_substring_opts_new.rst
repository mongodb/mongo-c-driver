:man_page: mongoc_encrypt_text_substring_opts_new

mongoc_encrypt_text_substring_opts_new()
========================================

Synopsis
--------

.. code-block:: c

   mongoc_encrypt_text_substring_opts_t *
   mongoc_encrypt_text_substring_opts_new(void);

.. versionadded:: 2.2.0

Returns
-------

A new :symbol:`mongoc_encrypt_text_substring_opts_t` that must be freed with :symbol:`mongoc_encrypt_text_substring_opts_destroy()`.

.. seealso::
   | :symbol:`mongoc_encrypt_text_substring_opts_destroy`
   | :symbol:`mongoc_encrypt_text_opts_set_substring`
