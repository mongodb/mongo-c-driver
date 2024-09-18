:man_page: mongoc_cursor_get_server_id

mongoc_cursor_get_server_id()
=============================

Synopsis
--------

.. code-block:: c

  uint32_t
  mongoc_cursor_get_server_id (const mongoc_cursor_t *cursor);

Parameters
----------

* ``cursor``: A :symbol:`mongoc_cursor_t`.

Description
-----------

Retrieves the opaque id of the server used for the operation.

This number is zero until the driver actually uses a server when executing the find operation or :symbol:`mongoc_cursor_set_server_id` is called.

