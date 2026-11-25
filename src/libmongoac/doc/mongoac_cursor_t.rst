:man_page: mongoac_cursor_t

mongoac_cursor_t
================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor-fwd.h>

   typedef struct mongoac_cursor_t mongoac_cursor_t;

   #include <mongoac/cursor.h>

Description
-----------

Represents a `mongodb::Cursor <https://docs.rs/mongodb/latest/mongodb/struct.Cursor.html>`_.

When the operation is associated with a :symbol:`mongoac_client_session_t`, represents a `mongodb::SessionCursor <https://docs.rs/mongodb/latest/mongodb/struct.SessionCursor.html>`_.
The associated client session used to create the cursor is used for all subsequent operations on the cursor.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_cursor_destroy
    mongoac_cursor_clone

    mongoac_cursor_next
    mongoac_cursor_next_async
