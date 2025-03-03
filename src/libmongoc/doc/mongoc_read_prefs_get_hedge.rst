:man_page: mongoc_read_prefs_get_hedge

mongoc_read_prefs_get_hedge()
=============================

.. deprecated:: MongoDB Server 8.0

  Hedged reads are deprecated in MongoDB version 8.0 and will be removed in
  a future release.

Synopsis
--------

.. code-block:: c

  const bson_t *
  mongoc_read_prefs_get_hedge (const mongoc_read_prefs_t *read_prefs);

Parameters
----------

* ``read_prefs``: A :symbol:`mongoc_read_prefs_t`.

Description
-----------

Fetches any read preference hedge document that has been registered.

Returns
-------

Returns a :symbol:`bson:bson_t` that should not be modified or freed.

