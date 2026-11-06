:man_page: mongoac_cursor_next

mongoac_cursor_next()
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor.h>

   mongoac_bson_t mongoac_cursor_next(mongoac_cursor_t *cursor, mongoac_error_t *error);

Description
-----------

Retrieve the next result from the given :symbol:`mongoac_cursor_t` via `Stream<T>::next() <https://docs.rs/mongodb/latest/mongodb/struct.Cursor.html#method.poll_next>`_.

When the cursor is exhausted, returns a null :symbol:`mongoac_bson_t`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``cursor`` is null.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_cursor_next_async()`
   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
