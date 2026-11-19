:man_page: mongoac_collection_replace_one

mongoac_collection_replace_one()
================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/collection.h>

   mongoac_update_result_t *mongoac_collection_replace_one(const mongoac_collection_t *collection,
                                                           mongoac_client_session_t *session,
                                                           mongoac_bson_view_t filter,
                                                           mongoac_bson_view_t replacement,
                                                           const mongoac_replace_options_t *options,
                                                           mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Replaces at most one document matching the given ``filter`` with the given ``replacement`` document via `Collection::replace_one() <https://docs.rs/mongodb/latest/mongodb/struct.Collection.html#method.replace_one>`_.

The ``replacement`` document must not contain update operators.

The returned :symbol:`mongoac_update_result_t` must be destroyed with :symbol:`mongoac_update_result_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``collection`` is null.
- Invalid Argument (mongoac): ``filter`` is null or invalid BSON.
- Invalid Argument (mongoac): ``replacement`` is null or invalid BSON.
- Server-Side Error (server, rust): the server returned an error.
- Client-Side Error (rust): the requested operation failed.

.. seealso::

   | :symbol:`mongoac_collection_replace_one_async()`
