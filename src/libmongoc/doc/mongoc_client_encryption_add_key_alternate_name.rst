:man_page: mongoc_client_encryption_add_key_alternate_name

mongoc_client_encryption_add_key_alternate_name()
=================================================

Synopsis
--------

.. code-block:: c

   MONGOC_EXPORT (bool)
   mongoc_client_encryption_add_key_alternate_name (
      mongoc_client_encryption_t *client_encryption,
      const bson_value_t *keyid,
      const char *keyaltname,
      bson_value_t *key_doc,
      bson_error_t *error);

Add ``keyaltname`` to the set of alternate names in the key document with UUID ``keyid``.

Parameters
----------

* ``client_encryption``: A :symbol:`mongoc_client_encryption_t`.
* ``keyid``: A UUID key ID of the key to add the key alternate name to.
* ``keyaltname``: The key alternate name to add.
* ``key_doc``: The value of the key document before addition of the alternate name, or ``null`` if a key with the given UUID does not exist. Must be freed by :symbol:`bson_value_destroy`.
* ``error``: Optional. :symbol:`bson_error_t`.

Returns
-------

Returns ``true`` if successful. Returns ``false`` and sets ``error`` otherwise.

.. seealso::

  | :symbol:`mongoc_client_encryption_t`
