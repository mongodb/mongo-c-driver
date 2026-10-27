:man_page: mongoac_insert_one_result_get_inserted_id

mongoac_insert_one_result_get_inserted_id()
===========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_one_result.h>

   mongoac_bson_t mongoac_insert_one_result_get_inserted_id(const mongoac_insert_one_result_t *result,
                                                            mongoac_error_t *error);

Description
-----------

Returns the ``insertedId`` of the ``insertOne`` operation as a BSON document of the form ``{"v": <value>}``.

The returned :symbol:`mongoac_bson_t` must be destroyed with :symbol:`mongoac_bson_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.
- Client-Side Error (bson): the inserted id could not be serialized.

.. seealso::

   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
