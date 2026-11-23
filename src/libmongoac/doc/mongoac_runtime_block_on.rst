:man_page: mongoac_runtime_block_on

mongoac_runtime_block_on()
==========================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   void mongoac_runtime_block_on(const mongoac_runtime_t *runtime,
                                 mongoac_future_t *future,
                                 mongoac_error_t *error);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

.. important::

   This function is a :ref:`polling function <polling-functions>`.

Description
-----------

Block the current thread until the given :symbol:`mongoac_future_t` is ready.

Returns immediately (without making progress) when the future is already ready.

Errors
------

This function may return the following errors:

- Invalid Argument (mongoac):
   - ``runtime`` is null.
   - ``future`` is null.
   - ``future`` is not associated with ``runtime``.

.. seealso::

   | :symbol:`mongoac_runtime_block_on_any`
   | :symbol:`mongoac_runtime_block_on_all`
   | :symbol:`mongoac_runtime_t`
