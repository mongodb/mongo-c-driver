:man_page: mongoac_insert_many_result_get_inserted_ids_len

mongoac_insert_many_result_get_inserted_ids_len()
=================================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/insert_many_result.h>

   uintptr_t mongoac_insert_many_result_get_inserted_ids_len(const mongoac_insert_many_result_t *result);

Description
-----------

Returns the number of inserted ids of the ``insertMany`` operation.

When ``result`` is null, returns zero.

.. seealso::

   | :symbol:`mongoac_insert_many_result_get_inserted_ids_get`
