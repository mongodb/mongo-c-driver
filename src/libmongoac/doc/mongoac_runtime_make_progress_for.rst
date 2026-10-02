:man_page: mongoac_runtime_make_progress_for

mongoac_runtime_make_progress_for()
===================================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   void mongoac_runtime_make_progress_for(const mongoac_runtime_t *runtime, uint64_t duration_ms);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Make progress on tasks spawned with the given runtime for *at least* the given duration.

When ``runtime`` is null, does nothing.

When ``duration_ms`` is too large and may overflow the representable range, the maximum representable duration is used (saturation).

This function blocks the current thread for *at least* the given duration:

.. code-block:: rust

   block_on(sleep(duration_ms))

.. seealso::

   | :symbol:`mongoac_runtime_make_progress`
   | :symbol:`mongoac_runtime_t`
