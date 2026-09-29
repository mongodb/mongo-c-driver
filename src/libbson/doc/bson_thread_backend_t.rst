:man_page: bson_thread_backend_t

bson_thread_backend_t
=====================

A table of callbacks for replacing thread and synchronization operations.
Install it with :symbol:`bson_set_thread_backend` for standalone libbson, or
``mongoc_set_thread_backend`` when using libmongoc.

Synopsis
--------

.. literalinclude:: ../src/bson/bson-thread-backend.h
   :language: c
   :start-at: typedef struct bson_thread_backend_t
   :end-at: } bson_thread_backend_t;

Contract
--------

The setter copies the table. The callback code and its supporting runtime must
remain available until all library use and cleanup have completed. Register from
a single thread before any other library operation. Do not change the backend
while synchronization objects exist, including after ``mongoc_cleanup``.
Callbacks must not call driver or libbson APIs recursively.

Set ``struct_size`` to ``sizeof(bson_thread_backend_t)`` when initializing the
table. The library uses this value to copy only the portion provided by the
application, allowing a newer library to accept a table built against an older
header. Fields beyond ``struct_size`` use their default (native) behavior.

A NULL callback selects the native implementation. Related callbacks must be
supplied together: thread create/join; all mutex and condition operations;
and all shared mutex operations. The library does not validate these groups.
``mutex_is_locked`` is used only in debug builds and must report whether the
calling thread holds the mutex.

All storage arguments identify driver-owned private objects. Their sizes,
alignment, and representations are not public. Backends may use a synchronized
map keyed by the storage address, but must not write their own representations
into that storage. Initialization callbacks need not receive zeroed storage.
Objects may reuse an address after destruction; remove the association on destroy.

Thread storage is an exception: ``thread_create`` writes a ``void *`` handle to
``*(void **)storage``, and ``thread_join`` receives a pointer to a copy of that
handle. Thread callbacks return zero on success and an errno-style code on
failure. Release backend resources after a successful join.
The thread callback type :symbol:`bson_thread_backend_function_t` preserves the
platform's calling convention.

Once storage has process lifetime. ``once`` must run its supplied function
exactly once, and concurrent callers must wait until initialization completes.
Mutexes are non-recursive. Condition waits atomically release and reacquire the
associated mutex, including on timeout. ``cond_timedwait`` takes a relative
millisecond duration, returns zero on success or a backend error code, and
``cond_is_timedout`` recognizes the timeout code. Shared mutexes permit concurrent
readers and exclusive writers. ``sleep`` takes microseconds; nonpositive values
must return immediately. Operations without an error result must succeed or
terminate. Once, locks, and join must provide the same memory visibility as the
native operations.

This API replaces the driver's threading abstraction. It does not replace
networking, DNS, TLS, third-party library internals, atomics, or native thread IDs
used for logging. A cooperative scheduler must account for those operations too.
