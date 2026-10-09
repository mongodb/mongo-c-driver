:man_page: mongoac_runtime_block_on_any_with_timeout

mongoac_runtime_block_on_any_with_timeout()
===========================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   mongoac_future_t *const *mongoac_runtime_block_on_any_with_timeout(const mongoac_runtime_t *runtime,
                                                                      mongoac_future_t *const *futures,
                                                                      uintptr_t count,
                                                                      uint64_t timeout_ms,
                                                                      mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

.. important::

   This function is a :ref:`polling function <polling-functions>`.

Description
-----------

Block the current thread, :ref:`up to the specified timeout <timeouts>`, until any :symbol:`mongoac_future_t` is ready.

Returns immediately (without making progress) when any future is already ready.

When a future is ready, returns an iterator to that future.

Preconditions
-------------

The following preconditions must be satisfied:

- When ``futures`` is not null:
   - ``futures`` must point to a valid array of at least ``count`` elements.
   - All elements in the array must point to a unique :symbol:`mongoac_future_t`.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac):
   - ``runtime`` is null.
   - ``futures`` is null or empty.
   - An element of ``futures`` is null.
   - An element of ``futures`` is not associated with ``runtime``.
- Timeout (mongoac): the deadline specified by ``timeout_ms`` was exceeded before any future was ready.

.. seealso::

   | :symbol:`mongoac_runtime_block_on_with_timeout`
   | :symbol:`mongoac_runtime_block_on_all_with_timeout`
   | :symbol:`mongoac_runtime_t`
