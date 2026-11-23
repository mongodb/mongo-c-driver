:man_page: mongoac_future_get_delete_result

mongoac_future_get_delete_result()
==================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   const mongoac_delete_result_t *mongoac_future_get_delete_result(const mongoac_future_t *future,
                                                                   mongoac_error_t *error);

Description
-----------

Obtain the :ref:`result <future-result>` of the given :symbol:`mongoac_future_t`.

Lifetime
--------

Access to the pointed-to result MUST be within the lifetime of the associated future.

Errors
------

In addition to errors produced by the async operation, this function may also return the following errors:

- Invalid Argument (mongoac):
   - ``future`` is null.
   - ``future`` does not return the specified type.
   - ``future`` is not ready.

.. seealso::

   | :symbol:`mongoac_future_is_ready`
   | :symbol:`mongoac_error_t`
   | :symbol:`mongoac_delete_result_t`
