:man_page: mongoc_client_encryption_remove_key_alternate_name

mongoc_client_encryption_remove_key_alternate_name()
====================================================

Synopsis
--------

.. code-block:: c

   MONGOC_EXPORT (bool)
   mongoc_client_encryption_remove_key_alternate_name (
      mongoc_client_encryption_t *client_encryption,
      const bson_value_t *keyid,
      const char *keyaltname,
      bson_value_t *key_doc,
      bson_error_t *error);

Remove ``keyaltname`` from the set of keyAltnames in the key document with UUID ``keyid``.

Also removes the ``keyAltNames`` field from the key document if it would otherwise be empty.

Parameters
----------

* ``client_encryption``: A :symbol:`mongoc_client_encryption_t`.
* ``keyid``: The UUID (BSON binary subtype 0x04) of the key to remove the key alternate name from.
* ``keyaltname``: The key alternate name to remove.
* ``key_doc``: The value of the key document before removal of the key alternate name, or ``null`` if a key with the given UUID does not exist. Must be freed by :symbol:`bson_value_destroy`.
* ``error``: Optional. :symbol:`bson_error_t`.

Returns
-------

Returns ``true`` if successful. Returns ``false`` and sets ``error`` otherwise.

.. seealso::

  | :symbol:`mongoc_client_encryption_t`
