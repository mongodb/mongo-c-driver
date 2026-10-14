:man_page: mongoac_collection_drop

mongoac_collection_drop()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   void mongoac_collection_drop(const mongoac_collection_t *coll,
                                mongoac_client_session_t *session,
                                const mongoac_drop_collection_options_t *options,
                                mongoac_error_t *error);

Description
-----------

Drops the collection via `Collection::drop() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.drop>`_.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``coll`` is null.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_drop_async()`
