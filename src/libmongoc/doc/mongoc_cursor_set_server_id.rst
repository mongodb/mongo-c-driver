:man_page: mongoc_cursor_set_server_id

mongoc_cursor_set_server_id()
=============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_cursor_set_server_id (mongoc_cursor_t *cursor, uint32_t server_id)

Parameters
----------

* ``cursor``: A :symbol:`mongoc_cursor_t`.
* ``server_id``: An opaque id identifying the server to use.

Description
-----------

Specifies which server to use for the operation. This function has an effect only if called before the find operation is executed.

Use ``mongoc_cursor_set_server_id`` only for building a language driver that wraps the C Driver. When writing applications in C, leave the server id unset and allow the driver to choose a suitable server from the find operation's read preference.

Returns
-------

Returns true on success. If any arguments are invalid, returns false and logs an error.

