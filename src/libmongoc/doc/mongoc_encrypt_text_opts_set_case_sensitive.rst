:man_page: mongoc_encrypt_text_opts_set_case_sensitive

mongoc_encrypt_text_opts_set_case_sensitive()
============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_opts_set_case_sensitive(
      mongoc_encrypt_text_opts_t *opts,
      bool case_sensitive);

.. versionadded:: 1.26.0

Sets whether text search is case sensitive.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_opts_t`.
* ``case_sensitive``: If true, text search is case sensitive.

.. seealso::
   | :symbol:`mongoc_encrypt_text_opts_t`
