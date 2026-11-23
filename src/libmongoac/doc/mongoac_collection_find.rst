:man_page: mongoac_collection_find

mongoac_collection_find()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_cursor_t *mongoac_collection_find(const mongoac_collection_t *collection,
                                             mongoac_client_session_t *session,
                                             mongoac_bson_view_t filter,
                                             const mongoac_find_options_t *options,
                                             mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Finds all documents matching the given ``filter`` via `Collection::find() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.find>`_.

The returned :symbol:`mongoac_cursor_t` must be destroyed with :symbol:`mongoac_cursor_destroy()`.

The ``options`` parameter must be ``NULL``.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_find_async()`
   | :symbol:`mongoac_cursor_t`
