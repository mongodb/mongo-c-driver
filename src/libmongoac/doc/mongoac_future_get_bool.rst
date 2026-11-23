:man_page: mongoac_future_get_bool

mongoac_future_get_bool()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   bool mongoac_future_get_bool(const mongoac_future_t *future, mongoac_error_t *error);

Description
-----------

Obtain the :ref:`result <future-result>` of the given :symbol:`mongoac_future_t`.

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
