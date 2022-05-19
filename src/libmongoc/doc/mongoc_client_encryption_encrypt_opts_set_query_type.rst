:man_page: mongoc_client_encryption_encrypt_opts_set_query_type

mongoc_client_encryption_encrypt_opts_set_query_type()
======================================================

Synopsis
--------

.. code-block:: c

   typedef enum { MONGOC_ENCRYPT_QUERY_TYPE_EQUALITY } mongoc_encrypt_query_type_t;

   MONGOC_EXPORT (void)
    mongoc_client_encryption_encrypt_opts_set_query_type (
        mongoc_client_encryption_encrypt_opts_t *opts, mongoc_encrypt_query_type_t query_type);

Sets a query type for explicit encryption.
Only applies when the algorithm set by :symbol:`mongoc_client_encryption_encrypt_opts_set_algorithm()` is "Indexed".
It is an error to set the query type when algorithm is not "Indexed".

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``query_type``: A query type to use for explicit encryption.
