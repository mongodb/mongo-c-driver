:man_page: mongoac_future_t

mongoac_future_t
================

Synopsis
--------

.. code-block:: c

   #include <mongoac/future-fwd.h>

   typedef struct mongoac_future_t mongoac_future_t;

   #include <mongoac/future.h>

Description
-----------

Represents a handle to an async operation.

The async operation is spawned with the :symbol:`mongoac_runtime_t` associated with the object from which the future is obtained.
The associated runtime may be obtained by calling :symbol:`mongoac_future_get_runtime()`.
Once the future is driven to completion, the result of the async operation is available via the appropriate getter function.

.. important::

   A future may only be driven using progress functions with its associated runtime.

.. _polling:

Polling
-------

A future is *pending* when its async operation has not yet produced a result.
The future must be *polled* in order to drive its async operation to completion.
When a future is *completed*, its result may be accessed using the appropriate getter function.

.. _polling-functions:

A *polling function* is a function which polls the given future(s).
Only :ref:`block-on functions <block-on-functions>` and :symbol:`mongoac_future_poll()` are polling functions.
Notably, :symbol:`mongoac_future_is_ready()` is *not* a polling function.

.. _readiness:

Readiness
---------

A future is *ready* when the async operation completed and produced a result.
The result of a future may only be accessed once the future is ready, using the appropriate getter function.
The getter function must match the return type of the async operation used to spawn the future.

.. _future-result:

Result
------

The *result* of a future may be one of the following:

- A *return value* produced by the async operation on success (completion).
- An *error* produced by the async operation on failure (completion).
- An *error* due to being unable to complete the async operation (panic or cancellation).

The return value is directly returned by the getter function.
The error is returned by setting the optional ``error`` parameter (and the return value is default-initialized).
When the requested return value does not match the type of the result produced by the async operation, ``MONGOAC_ERROR_CODE_INVALID_ARGUMENT`` error is returned.

.. code-block:: c

   void example(const mongoac_future_t* future, mongoac_error_t* error) {
      assert(mongoac_future_is_ready(future));

      uint64_t result = mongoac_future_get_uint64(future, error);

      // The return value alone may be insufficient to determine success or failure.
      if (mongoac_error_code(error) == MONGOAC_ERROR_CODE_NONE) {
         // `result` is the return value produced by the successful async operation.
         use(result);
      } else {
         // `error` is set to an error produced by the failed/panicked/cancelled async operation.
         // `result` is just a default-initialized value and not a meaningful value.
         assert(result == uint64_t{});
         use(error);
      }
   }

.. _cancellation:

Cancellation
------------

.. attention::

   **Cancellation is not supported!**

A pending future may be destroyed before its result is ready.
However, this will not unschedule the async operation from its associated runtime.
The async operation will continue to make progress until it is completed and its result is discarded.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoac_future_destroy
    mongoac_future_clone

    mongoac_future_get_bool
    mongoac_future_get_runtime
    mongoac_future_get_runtime_address
    mongoac_future_get_uint64
    mongoac_future_get_void
    mongoac_future_is_ready

    mongoac_future_poll
