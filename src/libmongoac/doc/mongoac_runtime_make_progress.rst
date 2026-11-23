:man_page: mongoac_runtime_make_progress

mongoac_runtime_make_progress()
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime.h>

   void mongoac_runtime_make_progress(const mongoac_runtime_t *runtime);

.. important::

   This function is a :ref:`progress function <progress-functions>`.

Description
-----------

Make progress on tasks spawned with the given runtime.

When ``runtime`` is null, does nothing.

Conceptually, this function makes a *single pass* through the runtime's task queue:

.. code-block:: rust

   block_on(yield_now())

.. important::

   This conceptual single pass does not guarantee that *every* task makes progress in a single invocation.
   However, every ready task will *eventually* make progress after repeated invocations, per `Tokio's fairness guarantee <https://docs.rs/tokio/latest/tokio/runtime/index.html#detailed-runtime-behavior>`_.

.. seealso::

   | :symbol:`mongoac_runtime_make_progress_for`
   | :symbol:`mongoac_runtime_t`
