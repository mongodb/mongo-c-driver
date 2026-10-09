:man_page: mongoac_future_is_ready

mongoac_future_is_ready()
=========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   bool mongoac_future_is_ready(const mongoac_future_t *future);

Description
-----------

When ``future`` is not null, return ``true`` when its :ref:`result is available <readiness>`.

Otherwise, return ``false``.

.. important::

   This is not a :ref:`polling function <polling-functions>`; invoking this function does not complete the future.
   However, it may observe readiness once the future is polled by another progress function.

.. seealso::

   | :symbol:`mongoac_future_poll`
   | :symbol:`mongoac_future_get_bool`
   | :symbol:`mongoac_future_get_uint64`
   | :symbol:`mongoac_future_get_void`
