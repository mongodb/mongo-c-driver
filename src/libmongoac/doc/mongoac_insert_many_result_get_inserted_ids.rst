:man_page: mongoac_insert_many_result_get_inserted_ids

mongoac_insert_many_result_get_inserted_ids()
=============================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_many_result.h>

   mongoac_bson_t mongoac_insert_many_result_get_inserted_ids(const mongoac_insert_many_result_t *result,
                                                              mongoac_error_t *error);

Description
-----------

Returns the ``insertedIds`` of the ``insertMany`` operation as a BSON array of the inserted ids in order of insertion index,
e.g. ``{"0": <value>, "1": <value>, ...}``.

The returned :symbol:`mongoac_bson_t` must be destroyed with :symbol:`mongoac_bson_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.
- Client-Side Error (bson): the inserted ids could not be serialized.

.. seealso::

   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
