:man_page: mongoac_update_result_get_modified_count

mongoac_update_result_get_modified_count()
==========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/update_result.h>

   uint64_t mongoac_update_result_get_modified_count(const mongoac_update_result_t *result,
                                                     mongoac_error_t *error);

Description
-----------

Returns the number of documents modified by the ``updateOne``, ``updateMany``, or ``replaceOne`` operation.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.

.. seealso::

   | :symbol:`mongoac_update_result_t`
   | :symbol:`mongoac_error_t`
