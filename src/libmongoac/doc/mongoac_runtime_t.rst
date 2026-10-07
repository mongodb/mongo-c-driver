:man_page: mongoac_runtime_t

mongoac_runtime_t
=================

Synopsis
--------

.. code-block:: c

   #include <mongoac/runtime-fwd.h>

   typedef struct mongoac_runtime_t mongoac_runtime_t;

   #include <mongoac/runtime.h>

Description
-----------

Represents a `Tokio runtime <https://docs.rs/tokio/latest/tokio/runtime/index.html>`_ with which a :symbol:`mongoac_client_t` was created.

.. tip::

  The runtime may outlive its associated client.

Runtime
-------

The runtime is a `single-threaded executor and scheduler <https://docs.rs/tokio/latest/tokio/runtime/index.html#current-thread-scheduler>`_ with `I/O <https://docs.rs/tokio/latest/tokio/runtime/struct.Builder.html#method.enable_io>`_ and `time <https://docs.rs/tokio/latest/tokio/runtime/struct.Builder.html#method.enable_time>`_ drivers enabled.

All tasks corresponding to operations performed by a :symbol:`mongoac_client_t` are *spawned* ("scheduled to run on") with the associated runtime with which the client was created.
These tasks may include background tasks spawned during client construction (e.g. CMAP, SDAM, etc.) and cleanup routines during resource destruction (e.g. ``endSessions``, ``killCursors``, etc.).
Tasks must be *driven* ("make progress") to completion using one or more *progress functions* as provided by the runtime.

.. warning::

  As a single-threaded executor, no scheduled task can make progress unless a progress function is invoked by the user.
  To prevent unexpected latency or staleness during operations, ensure that at least one progress function is invoked periodically.
  Long idle periods may negatively impact connection pooling, server monitoring, SRV polling, and various other behaviors dependent on timers and deadlines.

.. _progress-functions:

Progress Functions
------------------

A *progress function* is any synchronous function which may drive tasks spawned with a given runtime.

Progress functions are thread-safe but mutually exclusive: multiple threads may call any progress function in parallel, but only one thread will have progress ownership of the given runtime at any time.
As long as at least one progress function is invoked periodically, all ready tasks spawned with the runtime will eventually make progress, per `Tokio's fairness guarantee <https://docs.rs/tokio/latest/tokio/runtime/index.html#detailed-runtime-behavior>`_.
A given progress function may drive progress for tasks unrelated to the specific operation for which it was invoked.

.. warning::

  Invoking a progress function from within another progress function is undefined behavior!

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_runtime_destroy
    mongoac_runtime_clone

    mongoac_runtime_address

    mongoac_runtime_make_progress
    mongoac_runtime_make_progress_for
