Runtime thread backend (experimental)
=====================================

An application can replace the driver's thread and synchronization operations
without rebuilding libbson or libmongoc. Call ``mongoc_set_thread_backend``
with a ``bson_thread_backend_t`` before ``mongoc_init`` and before any other
driver or libbson operation. The setter configures both libraries and returns
false if ``mongoc_init`` has already run. Standalone libbson users can call
``bson_set_thread_backend``. Call either setter from a single thread while no
driver operations are in progress. The supplied table is copied, so it need
not remain allocated, but the callback code and its runtime must outlive
``mongoc_cleanup``.

Pass ``NULL`` before initialization to restore native operations. A table can
leave callbacks ``NULL`` to use the native operation, but mutex and condition
callbacks must be replaced together: native condition waits expect a native
mutex. Similarly, create/join, init/destroy, and lock/unlock callbacks must
be supplied as complete groups. ``mutex_is_locked`` is used only in debug
builds. Callback implementations must not call driver APIs recursively.

All storage arguments refer to driver-owned, private objects. Except for
thread storage, their sizes and representations are not public. A backend can
use a synchronized map keyed by the storage address to associate its own
objects. Initialization callbacks need not receive zeroed storage. Thread
storage is a ``void *`` handle: ``thread_create`` writes a handle to
``*(void **)storage`` and ``thread_join`` receives a copy of that handle's
storage. The backend releases its thread resources after a successful join.
Once storage has process lifetime; the once callback must run the supplied
function exactly once and make concurrent callers wait for its completion.

Thread callbacks return zero on success and an errno-style code on failure.
Mutex operations have non-recursive semantics. Condition waits atomically
release and reacquire the associated mutex. A timed wait takes a relative
millisecond duration and ``cond_is_timedout`` recognizes its timeout result.
Shared mutexes support concurrent readers and exclusive writers. Sleep takes
microseconds; nonpositive values should return immediately. Operations that
have no error return must succeed or terminate. Lock/unlock, once, and join
must provide the same memory visibility as their native counterparts.

A POSIX example is in ``src/common/tests/thread-backend-pthread.h`` with
runtime adapters in ``src/common/tests/thread-backend-runtime.h``. The example
uses allocated mutex, condition, and shared-mutex handles to exercise the
private-storage contract. It does not provide cooperative scheduling.

Validation
----------

Build the regular test executable and run the thread contract with and without
the runtime backend::

   cmake -D CMAKE_BUILD_TYPE=Debug -B cmake-build
   cmake --build cmake-build --target test-libmongoc
   MONGOC_TEST_SKIP_LIVE=on MONGOC_TEST_OFFLINE=on \
     ./cmake-build/src/libmongoc/test-libmongoc -d -f -l '/Thread/*'
   MONGOC_TEST_RUNTIME_BACKEND=1 MONGOC_TEST_SKIP_LIVE=on MONGOC_TEST_OFFLINE=on \
     ./cmake-build/src/libmongoc/test-libmongoc -d -f -l '/Thread/*'

This replaces the driver's thread abstraction. It does not replace OS
networking, DNS, TLS, third-party library internals, atomics, or native thread
IDs used for logging. A cooperative runtime must also account for those
potentially blocking operations and arrange for cleanup before its scheduler
shuts down.
