# libmongoac examples

Examples for the MongoDB Async C Driver (`libmongoac`), an asynchronous C API wrapping the
[MongoDB Rust Driver](https://www.mongodb.com/docs/drivers/rust/current/). `libmongoac` is
experimental; the API and ABI are subject to change.

## Building

In-tree, with the `mongo_c_driver_examples` target:

```bash
cmake -D ENABLE_MONGOAC=ON -B _build
cmake --build _build --target mongo_c_driver_examples
./_build/src/libmongoac/examples/queue-processing
```

Standalone, against an installed libmongoac — from this directory:

```bash
cmake -S . -B _build -D CMAKE_PREFIX_PATH=/path/to/mongoac-prefix
cmake --build _build
```

Requires CMake 3.23+, a C++17 compiler, and the installed `mongoac` and `bson` CMake packages. If
a shared library isn't found at run time, add `<prefix>/lib` to `LD_LIBRARY_PATH` (Linux) or
`DYLD_LIBRARY_PATH` (macOS).

## queue-processing

A skeleton of a query service: producer threads build `find` commands and start them, a worker
thread keeps many of them in flight and drives the runtime, and consumer threads verify the
replies.

```
producers -> input queue -> worker (one client, runtime, and pool) -> output queue -> consumers
```

Producers call `mongoac_database_run_command_async()` — `{find: ..., filter: {group: <n>},
batchSize: <n>, singleBatch: true}` — and hand the future to the worker's bounded input queue. The
worker keeps up to `k_max_in_flight` futures in flight: it drives the runtime with
`mongoac_runtime_block_on_all_with_timeout()`, harvests every future that became ready, and
refills from the queue. Consumers parse each reply with libbson and check `ok: 1`, `cursor.id ==
0`, and that the returned documents match the request's filter.

The collection `mongoac_example.queue_processing` is dropped and re-populated with `k_doc_count`
documents on every run (via `mongoac_collection_insert_many()`), so don't point the example at
data you care about.

Every tunable is a constant at the top of the file. Exit code 0 on success; 1 if any request was
lost, failed, or failed verification. The printed counters, elapsed time, and throughput are
informational — this is not a benchmark.

### What libmongoac requires of an application

- **BSON bytes in, bytes out.** Commands are `mongoac_bson_view_t` (`{ptr, len}`) over bytes the
  caller owns; the driver copies them before the call returns. Replies come back the same way:
  `mongoac_future_get_bson()` returns a view that borrows the future's storage, so copy it before
  destroying the future. `mongoac_string_t` is not NUL-terminated.
- **Somebody must drive the runtime.** Each client owns a single-threaded runtime; operations only
  progress inside `block_on*()`/`make_progress*()` calls, which also poll completed operations
  before their results can be read. If nothing drives a runtime, its monitoring and pool
  maintenance stall.
- **Destroy what you create.** Every `*_new()`/`*_get_*()`/`*_async()` result is owning and has a
  matching `_destroy()`. `mongoac_client_shutdown()` ends background tasks (e.g. `endSessions`)
  and must be called after the client's other objects are destroyed; the runtime may outlive its
  client.

### Threading

Only one thread can drive a given runtime at a time, so the worker owns its client exclusively.
Producers start operations on that client from their own threads and pass the futures to the
worker: futures are ref-counted handles that may cross threads, but a future can only be driven by
the runtime of the client it was started on. Each thread owns its `mongoac_error_t`.

The input queue is bounded, so producers block when the worker falls behind.
`k_wait_slice_ms` — the timeout passed to `block_on_all_with_timeout()`, and the duration of
`make_progress_for()` when the worker is idle — trades latency for overhead: it bounds how long
new work waits and how often the worker wakes. A timeout never cancels an operation.

### Alpha limitations

- No cursor API: `find` runs through `run_command_async` and returns only the first batch. The
  example sends `singleBatch: true` (and checks `cursor.id == 0`) so no server-side cursor is left
  open.
- No sessions: pass `NULL`. No bulk writes: seeding uses `insert_many`. No cancellation: dropping
  a pending future lets the operation run to completion and discards the result.
