:man_page: mongoc_read_prefs_set_tags

mongoc_read_prefs_set_tags()
============================

Synopsis
--------

.. code-block:: c

  void
  mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                              const bson_t *tags);

Parameters
----------

* ``read_prefs``: A :symbol:`mongoc_read_prefs_t`.
* ``tags``: A :symbol:`bson:bson_t`.

Description
-----------

Sets the tags to be used for the read preference. Only mongod instances matching these tags will be suitable for handling the request.

