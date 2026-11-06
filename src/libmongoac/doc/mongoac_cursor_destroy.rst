:man_page: mongoac_cursor_destroy

mongoac_cursor_destroy()
========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor.h>

   void mongoac_cursor_destroy(mongoac_cursor_t *cursor);

Description
-----------

Destroy the given :symbol:`mongoac_cursor_t` and decrement its shared state's reference counter.

When ``cursor`` is null, does nothing.

.. note::

   The shared state is refcounted: it is deallocated only when its reference count is reduced to zero.

.. seealso::

   | :symbol:`mongoac_cursor_clone`
   | :symbol:`mongoac_cursor_t`
