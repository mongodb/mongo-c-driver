:man_page: mongoac_insert_many_result_get_inserted_ids_get

mongoac_insert_many_result_get_inserted_ids_get()
=================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_many_result.h>

   mongoac_bson_t mongoac_insert_many_result_get_inserted_ids_get(const mongoac_insert_many_result_t *result,
                                                                  uintptr_t key,
                                                                  mongoac_error_t *error);

Description
-----------

Returns the inserted id at the given ``key``, the index of the corresponding document in the ``insertMany`` operation,
as a BSON document of the form ``{"v": <value>}``.

When ``key`` is not present in the ``insertedIds`` of the ``insertMany`` operation (e.g. the corresponding document
failed to insert), returns a null :symbol:`mongoac_bson_t`.

The returned :symbol:`mongoac_bson_t` must be destroyed with :symbol:`mongoac_bson_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.
- Client-Side Error (bson): the inserted id could not be serialized.

.. seealso::

   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
   | :symbol:`mongoac_insert_many_result_get_inserted_ids_len`
