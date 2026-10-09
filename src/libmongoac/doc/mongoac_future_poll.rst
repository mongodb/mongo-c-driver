:man_page: mongoac_future_poll

mongoac_future_poll()
=====================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future.h>

   bool mongoac_future_poll(mongoac_future_t *future);

.. important::

   This function is a :ref:`polling function <polling-functions>`.

Description
-----------

:ref:`Poll <polling-functions>` the given :symbol:`mongoac_future_t`.

When ``future`` is not null, return ``true`` when its async operation is complete and its result is available.
Otherwise, return ``false``.

.. important::

   This is not a :ref:`progress function <progress-functions>`; invoking this function does not make progress.
   However, it may observe completion once the future is driven by another progress function.

.. seealso::

   | :symbol:`mongoac_future_is_ready`
   | :symbol:`mongoac_runtime_block_on`
