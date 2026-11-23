:man_page: mongoac_future_get_cursor

mongoac_future_get_cursor()
===========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   const mongoac_cursor_t *mongoac_future_get_cursor(const mongoac_future_t *future,
                                                     mongoac_error_t *error);

Description
-----------

Obtain the :ref:`result <future-result>` of the given :symbol:`mongoac_future_t`.

Lifetime
--------

Access to the pointed-to cursor MUST be within the lifetime of the associated future.

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
   | :symbol:`mongoac_cursor_t`
