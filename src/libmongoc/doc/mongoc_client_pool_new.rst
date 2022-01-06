:man_page: mongoc_client_pool_new

mongoc_client_pool_new()
========================

Synopsis
--------

.. code-block:: c

  mongoc_client_pool_t *
  mongoc_client_pool_new (const mongoc_uri_t *uri) BSON_GNUC_WARN_UNUSED_RESULT;

This function creates a new :symbol:`mongoc_client_pool_t` using the :symbol:`mongoc_uri_t` provided.

Parameters
----------

* ``uri``: A :symbol:`mongoc_uri_t`.

Returns
-------

A newly allocated :symbol:`mongoc_client_pool_t` that should be freed with :symbol:`mongoc_client_pool_destroy()` when no longer in use. On error, ``NULL`` is returned and an error may be logged.

.. seealso::

  | :symbol:`mongoc_client_pool_new_with_error()`

