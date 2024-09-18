:man_page: mongoc_cursor_get_hint

mongoc_cursor_get_hint()
========================

.. warning::
   .. deprecated:: 1.28.0

      This function is deprecated and should not be used in new code.

      Please use :symbol:`mongoc_cursor_get_server_id()` in new code.

Synopsis
--------

.. code-block:: c

  uint32_t
  mongoc_cursor_get_hint (const mongoc_cursor_t *cursor) BSON_GNUC_DEPRECATED_FOR (mongoc_cursor_get_server_id);

Parameters
----------

* ``cursor``: A :symbol:`mongoc_cursor_t`.

Description
-----------

Retrieves the opaque id of the server used for the operation.

(The function name includes the old term "hint" for the sake of backward compatibility, but we now call this number a "server id".)

This number is zero until the driver actually uses a server when executing the find operation or :symbol:`mongoc_cursor_set_server_id` is called.

