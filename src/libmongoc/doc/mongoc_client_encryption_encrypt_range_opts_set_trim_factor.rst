:man_page: mongoc_client_encryption_encrypt_range_opts_set_trim_factor

mongoc_client_encryption_encrypt_range_opts_set_trim_factor()
=============================================================

Synopsis
--------

.. code-block:: c

    void
    mongoc_client_encryption_encrypt_range_opts_set_trim_factor (
         mongoc_client_encryption_encrypt_range_opts_t *range_opts, int32_t trim_factor);

.. versionadded:: 1.28.0

Sets trim factor for explicit encryption.
Only applies when the algorithm set by :symbol:`mongoc_client_encryption_encrypt_opts_set_algorithm()` is "Range".
It is an error to set trim factor when algorithm is not "Range".

The trim factor may be used to tune performance. When omitted, a default value is used.

Trim factor must match the value set in the encryptedFields of the destination collection.
It is an error to set a different value.

Parameters
----------

* ``range_opts``: A :symbol:`mongoc_client_encryption_encrypt_range_opts_t`
* ``trim_factor``: A non-negative trim factor.

.. seealso::
    | :symbol:`mongoc_client_encryption_encrypt_range_opts_t`
