:man_page: mongoac_collection_find_one

mongoac_collection_find_one()
=============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_bson_t mongoac_collection_find_one(const mongoac_collection_t *collection,
                                              mongoac_client_session_t *session,
                                              mongoac_bson_view_t filter,
                                              const mongoac_find_one_options_t *options,
                                              mongoac_error_t *error);

Description
-----------

Finds a single document matching the given ``filter`` via `Collection::find_one() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.find_one>`_.

The ``options`` parameter must be ``NULL``.

When no document matches the filter, returns a null :symbol:`mongoac_bson_t`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_find_one_async()`
