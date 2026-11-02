:man_page: mongoac_delete_result_get_deleted_count

mongoac_delete_result_get_deleted_count()
=========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/delete_result.h>

   uint64_t mongoac_delete_result_get_deleted_count(const mongoac_delete_result_t *result,
                                                    mongoac_error_t *error);

Description
-----------

Returns the number of documents deleted by the ``deleteOne`` operation.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac): ``result`` is null.

.. seealso::

   | :symbol:`mongoac_delete_result_t`
   | :symbol:`mongoac_error_t`
