:man_page: mongoac_runtime_block_on_any

mongoac_runtime_block_on_any()
==============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   mongoac_future_t *const *mongoac_runtime_block_on_any(const mongoac_runtime_t *runtime,
                                                         mongoac_future_t *const *futures,
                                                         uintptr_t count,
                                                         mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

.. important::

   This function is a :ref:`polling function <polling-functions>`.

Description
-----------

Block the current thread until any :symbol:`mongoac_future_t` is ready.

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

.. seealso::

   | :symbol:`mongoac_runtime_block_on`
   | :symbol:`mongoac_runtime_block_on_all`
   | :symbol:`mongoac_runtime_t`
