:man_page: mongoac_cursor_type_t

mongoac_cursor_type_t
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/cursor_type.h>

   typedef int32_t mongoac_cursor_type_t;

   // Cursor Types
   #define MONGOAC_CURSOR_TYPE_NON_TAILABLE 0
   #define MONGOAC_CURSOR_TYPE_TAILABLE 1
   #define MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT 2
   #define MONGOAC_CURSOR_TYPE_UNKNOWN /* (see below) */

Description
-----------

Represents a `mongodb::options::CursorType <https://docs.rs/mongodb/latest/mongodb/options/enum.CursorType.html>`_.

Cursor Types
------------

The following `cursor types <https://docs.rs/mongodb/latest/mongodb/options/enum.CursorType.html>`_ are supported:

- ``MONGOAC_CURSOR_TYPE_NON_TAILABLE`` ("nonTailable"): `CursorType::NonTailable <https://docs.rs/mongodb/latest/mongodb/options/enum.CursorType.html#variant.NonTailable>`_.
- ``MONGOAC_CURSOR_TYPE_TAILABLE`` ("tailable"): `CursorType::Tailable <https://docs.rs/mongodb/latest/mongodb/options/enum.CursorType.html#variant.Tailable>`_.
- ``MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT`` ("tailableAwait"): `CursorType::TailableAwait <https://docs.rs/mongodb/latest/mongodb/options/enum.CursorType.html#variant.TailableAwait>`_.
- ``MONGOAC_CURSOR_TYPE_UNKNOWN`` ("unknown"): Unknown value. Defaults to ``INT32_MIN`` unless otherwise specified.
