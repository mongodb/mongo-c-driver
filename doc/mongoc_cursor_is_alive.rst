:man_page: mongoc_cursor_is_alive

mongoc_cursor_is_alive()
========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_cursor_is_alive (const mongoc_cursor_t *cursor);

Parameters
----------

* ``cursor``: A :symbol:`mongoc_cursor_t`.

Description
-----------

Checks to see if a cursor is in a state that allows for more documents to be queried.

This is primarily useful with tailable cursors.

Returns
-------

true if there may be more content to retrieve from the cursor.

