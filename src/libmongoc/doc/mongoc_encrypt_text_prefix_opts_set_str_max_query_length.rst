:man_page: mongoc_encrypt_text_prefix_opts_set_str_max_query_length

mongoc_encrypt_text_prefix_opts_set_str_max_query_length()
==========================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_encrypt_text_prefix_opts_set_str_max_query_length(
      mongoc_encrypt_text_prefix_opts_t *opts,
      int32_t str_max_query_length);

.. versionadded:: 2.2.0

Sets the maximum query length for prefix text search encryption options.

Parameters
----------

* ``opts``: A :symbol:`mongoc_encrypt_text_prefix_opts_t`.
* ``str_max_query_length``: The maximum query length for prefix search. Must be greater than zero.

.. seealso::
   | :symbol:`mongoc_encrypt_text_prefix_opts_new`
   | :symbol:`mongoc_encrypt_text_opts_set_prefix`
