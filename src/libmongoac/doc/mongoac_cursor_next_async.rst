:man_page: mongoac_cursor_next_async

mongoac_cursor_next_async()
===========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor.h>

   mongoac_future_t *mongoac_cursor_next_async(mongoac_cursor_t *cursor);

Description
-----------

Async equivalent to :symbol:`mongoac_cursor_next()`.

When ``cursor`` is null, returns ``NULL``.

The returned :symbol:`mongoac_future_t` must be destroyed with :symbol:`mongoac_future_destroy()`.

.. note::

   The result of the async operation is obtained with :symbol:`mongoac_future_get_optional_bson()`, not :symbol:`mongoac_future_get_bson()`.

.. seealso::

   | :symbol:`mongoac_cursor_next()`
   | :symbol:`mongoac_future_get_optional_bson()`
