:man_page: mongoac_future_get_insert_one_result

mongoac_future_get_insert_one_result()
======================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   mongoac_insert_one_result_t *mongoac_future_get_insert_one_result(const mongoac_future_t *future,
                                                                     mongoac_error_t *error);

Description
-----------

Obtain the :ref:`result <future-result>` of the given :symbol:`mongoac_future_t`.

The returned :symbol:`mongoac_insert_one_result_t` must be destroyed with :symbol:`mongoac_insert_one_result_destroy()`.

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
   | :symbol:`mongoac_insert_one_result_t`
