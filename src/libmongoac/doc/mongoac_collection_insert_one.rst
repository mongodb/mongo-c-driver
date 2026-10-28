:man_page: mongoac_collection_insert_one

mongoac_collection_insert_one()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_insert_one_result_t *mongoac_collection_insert_one(const mongoac_collection_t *collection,
                                                              mongoac_client_session_t *session,
                                                              mongoac_bson_view_t doc,
                                                              const mongoac_insert_one_options_t *options,
                                                              mongoac_error_t *error);

Description
-----------

Inserts the given ``doc`` via `Collection::insert_one() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.insert_one>`_.

The returned :symbol:`mongoac_insert_one_result_t` must be destroyed with :symbol:`mongoac_insert_one_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``doc`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_insert_one_async()`
