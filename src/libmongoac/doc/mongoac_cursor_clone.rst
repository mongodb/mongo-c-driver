:man_page: mongoac_cursor_clone

mongoac_cursor_clone()
======================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor.h>

   mongoac_cursor_t *mongoac_cursor_clone(const mongoac_cursor_t *cursor);

Description
-----------

Returns a new :symbol:`mongoac_cursor_t` with the same shared state as ``cursor``.

When ``cursor`` is null, returns a null pointer.

Must be destroyed with :symbol:`mongoac_cursor_destroy()`.

.. note::

   The shared state is refcounted: cloning is cheap and increments the reference count.

.. seealso::

   | :symbol:`mongoac_cursor_destroy`
   | :symbol:`mongoac_cursor_next`
