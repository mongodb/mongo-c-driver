:man_page: mongoac_collection_delete_one

mongoac_collection_delete_one()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_delete_result_t *mongoac_collection_delete_one(const mongoac_collection_t *collection,
                                                          mongoac_client_session_t *session,
                                                          mongoac_bson_view_t filter,
                                                          const mongoac_delete_options_t *options,
                                                          mongoac_error_t *error);

Description
-----------

Deletes at most one document matching the given ``filter`` via `Collection::delete_one() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.delete_one>`_.

The returned :symbol:`mongoac_delete_result_t` must be destroyed with :symbol:`mongoac_delete_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_delete_one_async()`
