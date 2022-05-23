:man_page: mongoc_client_encryption_get_key_by_alt_name

mongoc_client_encryption_get_key_by_alt_name()
==============================================

Synopsis
--------

.. code-block:: c

   struct _mongoc_cursor_t *
   mongoc_client_encryption_get_key_by_alt_name (
      mongoc_client_encryption_t *client_encryption,
      const char *keyaltname,
      bson_error_t *error);

Get a key document in the key vault collection that has the given ``keyaltname``.

Parameters
----------

* ``client_encryption``: A :symbol:`mongoc_client_encryption_t`.
* ``keyaltname``: The key alternate name of the key to get.
* ``error``: Optional. :symbol:`bson_error_t`.

Returns
-------

Returns the result of the internal find command if successful. Returns ``NULL`` and sets ``error`` otherwise.

.. seealso::

  | :symbol:`mongoc_client_encryption_t`
