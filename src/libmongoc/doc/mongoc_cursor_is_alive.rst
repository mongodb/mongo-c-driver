:man_page: mongoc_cursor_is_alive

mongoc_cursor_is_alive()
========================

.. warning::
   .. deprecated:: 1.10.0

      Use :symbol:`mongoc_cursor_more()` instead.


Synopsis
--------

.. code-block:: c

  bool
  mongoc_cursor_is_alive (const mongoc_cursor_t *cursor);

Parameters
----------

* ``cursor``: A :symbol:`mongoc_cursor_t`.


Returns
-------

See :symbol:`mongoc_cursor_more()`.
