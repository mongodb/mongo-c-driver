:man_page: mongoc_client_encryption_encrypt_opts_set_query_type

mongoc_client_encryption_encrypt_opts_set_query_type()
======================================================

Synopsis
--------

.. code-block:: c

   #define MONGOC_ENCRYPT_QUERY_TYPE_EQUALITY "equality"

   MONGOC_EXPORT (void)
    mongoc_client_encryption_encrypt_opts_set_query_type (
        mongoc_client_encryption_encrypt_opts_t *opts, const char* query_type);

.. important:: |qenc:api-is-experimental|
.. versionadded:: 1.22.0

Sets a query type for explicit encryption. Currently, the only supported value
for ``query_type`` is ``"equality"``.

Only applies when the algorithm set by :symbol:`mongoc_client_encryption_encrypt_opts_set_algorithm()` is "Indexed".
It is an error to set the query type when algorithm is not "Indexed".

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``query_type``: A query type to use for explicit encryption.
