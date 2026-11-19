:man_page: mongoac_update_result_get_upserted_id

mongoac_update_result_get_upserted_id()
=======================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/update_result.h>

   mongoac_bson_t mongoac_update_result_get_upserted_id(const mongoac_update_result_t *result,
                                                        mongoac_error_t *error);

Description
-----------

Returns the ``upsertedId`` of the ``updateOne``, ``updateMany``, or ``replaceOne`` operation as a BSON document of the form ``{"v": <value>}``.

When no document was upserted, returns a null :symbol:`mongoac_bson_t`.

The returned :symbol:`mongoac_bson_t` must be destroyed with :symbol:`mongoac_bson_destroy()`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.
- Client-Side Error (bson): the upserted id could not be serialized.

.. seealso::

   | :symbol:`mongoac_bson_t`
   | :symbol:`mongoac_error_t`
   | :symbol:`mongoac_update_result_t`
