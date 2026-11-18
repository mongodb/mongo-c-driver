// Copyright 2009-present MongoDB, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// queue-processing.cpp
//
// A realistic skeleton of a high-throughput service built on libmongoac (the MongoDB Async C Driver). Producer threads
// start requests onto an in-memory queue, a dedicated worker thread runs each request as an asynchronous `find` with
// many operations in flight at once, and consumer threads drain the results.
//
// This example demonstrates the intended usage pattern for libmongoac:
//
// producers -> input queue -> worker -> output queue -> consumers (one client, runtime, and connection pool)
//
// It also demonstrates what the driver requires of the application:
//
// * BSON bytes in, BSON bytes out. Documents are built and parsed with libbson; the driver only moves raw BSON bytes.
// Views returned by the driver borrow driver-owned storage and must be copied before the owning object is destroyed.
// * A thread must drive the runtime. Asynchronous operations only make progress inside `mongoac_runtime_block_on*()`
// and `mongoac_runtime_make_progress*()` calls.
// * Destroy what you create. Every owning mongoac pointer or value has a matching `mongoac_*_destroy()` function.
//
// See README.md for build instructions, the threading rules, and the wait-slice trade-off.

#include <mongoac/bson.h>
#include <mongoac/client.h>
#include <mongoac/collection.h>
#include <mongoac/database.h>
#include <mongoac/error.h>
#include <mongoac/future.h>
#include <mongoac/insert_many_result.h>
#include <mongoac/runtime.h>
#include <mongoac/string.h>

#include <bson/bson.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------------------------
// Constants
//
// This example uses hardcoded values instead of command-line options. Adjust them to explore the pattern.

// Database and collection names. The collection is dropped and re-populated on every run.
constexpr char const* const k_database = "mongoac_example";
constexpr char const* const k_collection = "queue_processing";

// Documents to insert, and requests to process.
constexpr std::uint64_t k_doc_count = 10000;
constexpr std::uint64_t k_requests = 5000;

// Number of distinct values of the `group` field (and of the `filter: {group: ...}` queries).
constexpr std::uint32_t k_groups = 10;

// Producer and consumer thread counts. Each producer produces an equal share of the requests.
constexpr std::uint32_t k_producers = 2;
constexpr std::uint32_t k_consumers = 2;

static_assert(k_requests % k_producers == 0);

// Find operations in flight: also the input queue's capacity, so producers block when the worker falls behind. On the
// order of 100-200, per the goals for this example.
constexpr std::uint32_t k_max_in_flight = 128;

// Runtime wait slice, in milliseconds. See "The wait slice" in README.md.
constexpr std::uint64_t k_wait_slice_ms = 10;

// `find` batch size. The first batch is all this API returns; see README.md.
constexpr std::uint32_t k_batch_size = 20;

// Number of documents per `insert` command when populating the collection.
constexpr std::uint64_t k_insert_batch = 500;

// ---------------------------------------------------------------------------------------------
// Ownership helpers
//
// The driver hands out owning pointers (destroyed with `mongoac_*_destroy(ptr)`) and owning values (destroyed with
// `mongoac_*_destroy(value)`). Both fit in a `std::unique_ptr` with a function-pointer deleter, which is the only C++
// abstraction this example adds on top of the driver's C API.

template <typename T>
using owning_ptr = std::unique_ptr<T, void (*)(T*)>;

// RAII wrapper for owning driver values destroyed by value, e.g. `mongoac_string_t`.
template <typename T, void (*Destroy)(T)>
class owning_value {
   public:
    owning_value() = default;

    explicit owning_value(T value) noexcept : _value{value} {}

    ~owning_value() {
        Destroy(_value);
    }

    owning_value(owning_value const&) = delete;
    owning_value& operator=(owning_value const&) = delete;

    owning_value(owning_value&& other) noexcept : _value{other._value} {
        other._value = T{};
    }

    owning_value& operator=(owning_value&& other) noexcept {
        if (this != &other) {
            Destroy(_value);
            _value = other._value;
            other._value = T{};
        }
        return *this;
    }

    T const& get() const noexcept {
        return _value;
    }

   private:
    T _value{};
};

using owning_string = owning_value<mongoac_string_t, &mongoac_string_destroy>;

// ---------------------------------------------------------------------------------------------
// Error helpers
//
// mongoac reports errors through an out-parameter instead of a special return value. Note that
// `mongoac_error_message()` returns an owning `mongoac_string_t` that must be destroyed, and that `mongoac_string_t` is
// NOT NUL-terminated: use `ptr` and `len`.

owning_ptr<mongoac_error_t> make_error() {
    return {mongoac_error_new(), mongoac_error_destroy};
}

std::string error_text(mongoac_error_t const* error) {
    owning_string const message{mongoac_error_message(error)};

    if (!message.get().ptr) {
        return "<no error message>";
    }
    return std::string{message.get().ptr, message.get().len};
}

// Throw unless `error` holds `MONGOAC_ERROR_CODE_OK`.
void check_ok(mongoac_error_t* error, std::string_view what) {
    if (mongoac_error_code(error) != MONGOAC_ERROR_CODE_OK) {
        throw std::runtime_error{std::string{what} + ": " + error_text(error)};
    }
}

// ---------------------------------------------------------------------------------------------
// A minimal thread-safe queue
//
// The queue is bounded: a producer blocks while the queue is full.

template <typename T>
class queue {
   public:
    explicit queue(std::size_t capacity) : _state{std::make_unique<state>(capacity)} {}

    queue(queue&&) = default;
    queue& operator=(queue&&) = default;

    // Push an item, blocking while the queue is full. Returns `false` — without pushing — if the queue is closed, so
    // that a producer is never left blocking on a queue whose consumer has stopped.
    bool push(T value) {
        {
            std::unique_lock<std::mutex> lock{_state->mutex};
            _state->not_full.wait(lock, [this] { return _state->items.size() < _state->capacity || _state->closed; });
            if (_state->closed) {
                return false;
            }
            _state->items.push_back(std::move(value));
        }
        _state->not_empty.notify_one();
        return true;
    }

    // Non-blocking; returns nullopt when the queue is empty.
    std::optional<T> try_pop() {
        std::optional<T> value;
        {
            std::unique_lock<std::mutex> const lock{_state->mutex};
            value = pop_locked();
        }
        if (value) {
            _state->not_full.notify_one(); // A producer may be waiting for space.
        }
        return value;
    }

    // Blocks until an item is available or the queue is closed and drained.
    std::optional<T> pop() {
        std::optional<T> value;
        {
            std::unique_lock<std::mutex> lock{_state->mutex};
            _state->not_empty.wait(lock, [this] { return !_state->items.empty() || _state->closed; });
            value = pop_locked();
        }
        if (value) {
            _state->not_full.notify_one(); // A producer may be waiting for space.
        }
        return value;
    }

    // No more items will be pushed.
    void close() {
        {
            std::unique_lock<std::mutex> const lock{_state->mutex};
            _state->closed = true;
        }
        _state->not_empty.notify_all();
        _state->not_full.notify_all();
    }

    // True when the queue is closed and fully drained.
    bool is_finished() {
        std::unique_lock<std::mutex> const lock{_state->mutex};
        return _state->closed && _state->items.empty();
    }

    // True when the queue is closed (items may remain).
    bool is_closed() {
        std::unique_lock<std::mutex> const lock{_state->mutex};
        return _state->closed;
    }

   private:
    std::optional<T> pop_locked() {
        if (_state->items.empty()) {
            return std::nullopt;
        }

        T value = std::move(_state->items.front());
        _state->items.pop_front();
        return value;
    }

    struct state {
        explicit state(std::size_t capacity) : capacity{capacity} {}

        std::size_t const capacity;
        std::mutex mutex;
        std::condition_variable not_empty;
        std::condition_variable not_full;
        std::deque<T> items;
        bool closed = false;
    };

    // The synchronization state lives behind a pointer so that the queue is movable (a moved-from queue must not be
    // used).
    std::unique_ptr<state> _state;
};

// ---------------------------------------------------------------------------------------------
// Work items and results

struct request {
    std::uint64_t id = 0;
    std::uint32_t group = 0;
    // Expected number of documents in the first batch.
    std::uint32_t expected_doc_count = 0;
    // The operation started by the producer thread, driven to completion by a worker thread.
    owning_ptr<mongoac_future_t> future{nullptr, &mongoac_future_destroy};
};

struct result {
    std::uint64_t id = 0;
    std::uint32_t group = 0;
    std::uint32_t expected_doc_count = 0;
    // Empty on success; otherwise the error message produced by the failed operation.
    std::string error;
    // Owning copy of the reply document: the view returned by the driver borrows the future's
    // storage, and the future is destroyed before the consumer runs. Only set on success.
    std::vector<std::uint8_t> reply;
};

// ---------------------------------------------------------------------------------------------
// BSON helpers

mongoac_string_view_t to_view(std::string_view str) {
    return mongoac_string_view_t{str.data(), str.size()};
}

mongoac_bson_view_t to_view(bson_t const& bson) {
    return mongoac_bson_view_t{bson_get_data(&bson), bson.len};
}

mongoac_bson_view_t to_view(std::vector<std::uint8_t> const& bytes) {
    return mongoac_bson_view_t{bytes.data(), bytes.size()};
}

// Suppress noisy `-Wignored-attributes` warnings due to using `bson_t` for template arguments.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
using unique_bson_ptr = std::unique_ptr<bson_t, void (*)(bson_t*)>;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// `{find: <collection>, filter: {group: <group>}, batchSize: <n>, singleBatch: true}`
//
// `singleBatch: true` is required here: the alpha has no cursor API, so there is no way to iterate a cursor or send
// `killCursors`. `singleBatch` makes the server return the first batch and close the cursor, and the consumer verifies
// `cursor.id == 0` in every reply.
std::vector<std::uint8_t> build_find_command(char const* collection, std::uint32_t group, std::uint32_t batch_size) {
    unique_bson_ptr const command{
        BCON_NEW(
            "find",
            BCON_UTF8(collection),
            "filter",
            "{",
            "group",
            BCON_INT32(static_cast<std::int32_t>(group)),
            "}",
            "batchSize",
            BCON_INT32(static_cast<std::int32_t>(batch_size)),
            "singleBatch",
            BCON_BOOL(true)),
        &bson_destroy};

    return std::vector<std::uint8_t>{bson_get_data(command.get()), bson_get_data(command.get()) + command->len};
}

std::optional<double> iter_as_double(bson_iter_t const& iter) {
    if (BSON_ITER_HOLDS_DOUBLE(&iter)) {
        return bson_iter_double(&iter);
    }
    if (BSON_ITER_HOLDS_INT32(&iter) || BSON_ITER_HOLDS_INT64(&iter)) {
        return static_cast<double>(bson_iter_as_int64(&iter));
    }
    return std::nullopt;
}

std::optional<std::int64_t> iter_as_int(bson_iter_t const& iter) {
    if (BSON_ITER_HOLDS_INT32(&iter) || BSON_ITER_HOLDS_INT64(&iter)) {
        return bson_iter_as_int64(&iter);
    }
    if (BSON_ITER_HOLDS_DOUBLE(&iter)) {
        return static_cast<std::int64_t>(bson_iter_double(&iter));
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------
// Reply verification (consumers)
//
// This is application-level work: the driver moves bytes, the application decides what they mean. Every reply is
// checked so that a run is self-validating.
std::uint32_t check_find_reply(
    std::vector<std::uint8_t> const& bytes,
    std::uint32_t group,
    std::uint32_t expected_doc_count,
    std::uint32_t batch_size) {
    bson_t reply = BSON_INITIALIZER;
    if (!bson_init_static(&reply, bytes.data(), bytes.size())) {
        throw std::runtime_error{"reply is not valid BSON"};
    }

    bson_iter_t iter;
    if (!bson_iter_init_find(&iter, &reply, "ok")) {
        throw std::runtime_error{"reply has no 'ok' field"};
    }
    auto const ok_value = iter_as_double(iter);
    if (!ok_value || *ok_value != 1.0) {
        throw std::runtime_error{"reply 'ok' is not 1"};
    }

    bson_iter_t id_iter;
    bson_iter_init(&iter, &reply);
    if (!bson_iter_find_descendant(&iter, "cursor.id", &id_iter)) {
        throw std::runtime_error{"reply has no 'cursor.id' field"};
    }
    auto const cursor_id = iter_as_int(id_iter);
    if (!cursor_id) {
        throw std::runtime_error{"cursor 'id' is not a number"};
    }
    if (*cursor_id != 0) {
        throw std::runtime_error{"cursor 'id' is not 0: the server left a cursor open, which this API cannot close"};
    }

    bson_iter_t batch_iter;
    bson_iter_init(&iter, &reply);
    if (!bson_iter_find_descendant(&iter, "cursor.firstBatch", &batch_iter) || !BSON_ITER_HOLDS_ARRAY(&batch_iter)) {
        throw std::runtime_error{"reply has no 'cursor.firstBatch' array"};
    }

    std::uint32_t array_len = 0;
    std::uint8_t const* array_data = nullptr;
    bson_iter_array(&batch_iter, &array_len, &array_data);

    bson_t first_batch = BSON_INITIALIZER;
    if (!bson_init_static(&first_batch, array_data, array_len)) {
        throw std::runtime_error{"'firstBatch' is not valid BSON"};
    }

    std::uint32_t doc_count = 0;

    bson_iter_t element;
    bson_iter_init(&element, &first_batch);
    while (bson_iter_next(&element)) {
        if (!BSON_ITER_HOLDS_DOCUMENT(&element)) {
            throw std::runtime_error{"'firstBatch' contains a non-document element"};
        }

        bson_iter_t doc;
        bson_iter_recurse(&element, &doc);

        if (!bson_iter_find(&doc, "group")) {
            throw std::runtime_error{"document has no 'group' field"};
        }
        auto const doc_group = iter_as_int(doc);
        if (!doc_group || *doc_group != group) {
            throw std::runtime_error{"document does not match the request's filter"};
        }

        ++doc_count;
    }

    if (doc_count > batch_size) {
        throw std::runtime_error{"'firstBatch' is larger than 'batchSize'"};
    }

    if (doc_count != expected_doc_count) {
        throw std::runtime_error{
            "expected " + std::to_string(expected_doc_count) + " documents, got " + std::to_string(doc_count)};
    }

    return doc_count;
}

// ---------------------------------------------------------------------------------------------
// Worker

// The resources of one worker thread: its own client, the database and runtime handles associated with that client, the
// queue of operations started by the producers, and the queue of completed results for the consumers.
class worker {
   public:
    // Create the client and the database and runtime handles associated with it, and start the worker thread.
    explicit worker(std::string_view uri);

    ~worker();

    // Stop the worker and tear the driver objects down: close the input queue (no more operations), join the worker
    // thread (it drains the in-flight operations and exits), close the output queue (no more results), then destroy the
    // objects associated with the client, shutdown and destroy the client, and destroy the runtime (which may outlive
    // its client). `mongoac_client_shutdown()` drives the background tasks (e.g. `endSessions`) that client destruction
    // spawns, and may block indefinitely if other objects associated with the client still exist.
    void shutdown();

    // Producer side: the queue of operations for this worker to drive. Producers start operations on this worker's
    // client and hand the futures here. Pushing blocks while the queue is full; pushing to a closed queue returns
    // `false` without pushing, so that a producer is never left blocking on a queue whose worker has stopped.
    queue<request>& get_input() {
        return _input;
    }

    // Consumer side: the queue of completed results for the consumers to verify and count.
    queue<result>& get_output() {
        return _output;
    }

    std::string_view get_failure() const {
        return _failure;
    }

    mongoac_database_t* get_database() const {
        return _database.get();
    }

   private:
    void fail(std::string message) {
        _failure = std::move(message);
        _input.close();
    }

    void run();

    void harvest(request req);

    owning_ptr<mongoac_client_t> _client{nullptr, &mongoac_client_destroy};
    owning_ptr<mongoac_database_t> _database{nullptr, &mongoac_database_destroy};
    owning_ptr<mongoac_runtime_t> _runtime{nullptr, &mongoac_runtime_destroy};

    queue<request> _input{k_max_in_flight};

    queue<result> _output{k_max_in_flight};

    std::string _failure;

    // The id assigned to the next operation; used only on the worker thread.
    std::uint64_t _next_id_count = 0;

    std::thread _thread;
};

worker::worker(std::string_view uri) {
    auto const error = make_error();

    _client.reset(mongoac_client_new(to_view(uri), error.get()));
    if (!_client) {
        throw std::runtime_error{"mongoac_client_new: " + error_text(error.get())};
    }

    _database.reset(mongoac_client_get_database(_client.get(), to_view(k_database), nullptr));
    if (!_database) {
        throw std::runtime_error{"mongoac_client_get_database returned null"};
    }

    _runtime.reset(mongoac_client_get_runtime(_client.get()));
    if (!_runtime) {
        throw std::runtime_error{"mongoac_client_get_runtime returned null"};
    }

    _thread = std::thread(&worker::run, this);
}

worker::~worker() {
    shutdown();
}

void worker::shutdown() {
    // No more operations will be enqueued: the worker drains the in-flight operations and exits.
    _input.close();

    if (_thread.joinable()) {
        _thread.join();
    }

    // No more results will be produced.
    _output.close();

    _database.reset();
    mongoac_client_shutdown(_client.get());
    _client.reset();
    _runtime.reset();
}

// ---------------------------------------------------------------------------------------------
// Worker thread

// Take a completed request and queue its result on the output queue.
void worker::harvest(request req) {
    result res;
    res.id = req.id;
    res.group = req.group;
    res.expected_doc_count = req.expected_doc_count;

    auto const error = make_error();
    auto const reply = mongoac_future_get_bson(req.future.get(), error.get());

    if (mongoac_error_code(error.get()) == MONGOAC_ERROR_CODE_OK && reply.ptr) {
        res.reply.assign(reply.ptr, reply.ptr + reply.len);
    } else {
        res.error = error_text(error.get());
    }

    // The output queue is only closed by shutdown(), after this thread is joined, so the push always succeeds.
    BSON_ASSERT(_output.push(std::move(res)));
}

// The loop of the worker thread: move queued operations into the in-flight window, drive the runtime, and hand
// completed results to the output queue.
void worker::run() {
    auto* const runtime = _runtime.get();

    auto const error = make_error();

    std::vector<request> in_flight;
    std::vector<mongoac_future_t*> futures;

    try {
        for (;;) {
            // Keep the in-flight window full with the operations started by the producers. The
            // worker assigns each operation its id, in the order the operations are picked up.
            while (in_flight.size() < k_max_in_flight) {
                auto req = _input.try_pop();
                if (!req) {
                    break;
                }
                req->id = _next_id_count++;
                in_flight.push_back(std::move(*req));
            }

            if (in_flight.empty()) {
                if (_input.is_finished()) {
                    break; // All operations for this worker have been driven to completion.
                }

                // Idle: keep driving the runtime so that background tasks do not go stale, and bound how long a newly
                // queued operation can wait.
                mongoac_runtime_make_progress_for(runtime, k_wait_slice_ms);
                continue;
            }

            // Drive the runtime until every in-flight operation completes, or the wait slice expires. Only one thread
            // drives a given runtime at a time; this worker is the only thread using this runtime.
            futures.clear();
            for (auto const& entry : in_flight) {
                futures.push_back(entry.future.get());
            }

            mongoac_runtime_block_on_all_with_timeout(
                runtime, futures.data(), futures.size(), k_wait_slice_ms, error.get());

            switch (mongoac_error_code(error.get())) {
                case MONGOAC_ERROR_CODE_OK:
                    break; // Every in-flight operation completed.
                case MONGOAC_ERROR_CODE_TIMEOUT:
                    break; // Not all completed within the slice; harvest the ones that did.
                default:
                    throw std::runtime_error{"block_on_all_with_timeout: " + error_text(error.get())};
            }

            for (auto it = in_flight.begin(); it != in_flight.end();) {
                if (mongoac_future_is_ready(it->future.get())) {
                    auto req = std::move(*it);
                    it = in_flight.erase(it);
                    harvest(std::move(req));
                } else {
                    ++it;
                }
            }
        }
    } catch (std::exception const& exc) {
        // Unexpected driver failure: stop accepting operations and let main() report it.
        fail(exc.what());
    }
}

// ---------------------------------------------------------------------------------------------
// Setup (main thread, before the worker starts)

std::uint64_t count_in_group(std::uint64_t doc_count, std::uint32_t group_count, std::uint32_t group) {
    return doc_count / group_count + (group < doc_count % group_count ? 1 : 0);
}

// Clear and re-populate the collection, so that its contents are known exactly and every request can verify the number
// of documents in its first batch.
void populate_collection(mongoac_database_t* database) {
    auto const error = make_error();

    owning_ptr<mongoac_collection_t> const collection{
        mongoac_database_get_collection(database, to_view(k_collection), nullptr), &mongoac_collection_destroy};
    if (!collection) {
        throw std::runtime_error{"mongoac_database_get_collection returned null"};
    }

    mongoac_collection_drop(collection.get(), nullptr, nullptr, error.get());
    check_ok(error.get(), "drop");

    for (std::uint64_t first = 0; first < k_doc_count; first += k_insert_batch) {
        auto const count = std::min<std::uint64_t>(k_insert_batch, k_doc_count - first);

        std::vector<unique_bson_ptr> docs;
        std::vector<mongoac_bson_view_t> views;
        docs.reserve(static_cast<std::size_t>(count));
        views.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            auto const value = static_cast<std::int32_t>(first + i);

            docs.emplace_back(bson_new(), &bson_destroy);
            bson_t* const doc = docs.back().get();
            BSON_APPEND_INT32(doc, "_id", value);
            BSON_APPEND_INT32(doc, "group", value % static_cast<std::int32_t>(k_groups));
            BSON_APPEND_UTF8(doc, "payload", "example payload data");

            views.push_back(to_view(*doc));
        }

        owning_ptr<mongoac_insert_many_result_t> const result{
            mongoac_collection_insert_many(
                collection.get(), nullptr, mongoac_bson_view_array_t{views.data(), views.size()}, nullptr, error.get()),
            &mongoac_insert_many_result_destroy};

        check_ok(error.get(), "insert_many");
        auto const inserted_count = mongoac_insert_many_result_get_inserted_ids_len(result.get());
        if (inserted_count != count) {
            throw std::runtime_error{
                "insert_many inserted " + std::to_string(inserted_count) + " of " + std::to_string(count) +
                " documents"};
        }

        std::cout << "inserted documents " << first << ".." << (first + count - 1) << '\n';
    }
}

// ---------------------------------------------------------------------------------------------
// Producer and consumer threads

// A producer thread decides what operations to perform (here, a `find`) and starts them on the given database, then
// hands each operation's future to the given input queue.
class producer {
   public:
    producer(mongoac_database_t* database, queue<request>& input) : _database{database}, _input{input} {}

    // The loop of a producer thread. Each producer produces an equal share of the requests. Stops
    // early if the queue is closed (the worker stopped accepting operations). Called on a producer
    // thread.
    void run() {
        for (std::uint64_t id = 0; id < k_requests / k_producers && !_input.is_closed(); ++id) {
            request req;
            req.group = static_cast<std::uint32_t>(id % k_groups);
            req.expected_doc_count = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(count_in_group(k_doc_count, k_groups, req.group), k_batch_size));

            auto const command = build_find_command(k_collection, req.group, k_batch_size);
            req.future.reset(mongoac_database_run_command_async(_database, nullptr, to_view(command), nullptr));
            // `run_command_async()` only returns null on invalid arguments: a null database pointer, a null BSON view,
            // or invalid BSON. None can occur here.
            BSON_ASSERT(req.future != nullptr);

            if (!_input.push(std::move(req))) {
                break; // The worker stopped accepting operations.
            }
            ++_produced_count;
        }
    }

    std::uint64_t get_produced_count() const {
        return _produced_count;
    }

   private:
    mongoac_database_t* _database;
    queue<request>& _input;
    std::uint64_t _produced_count = 0;
};

// A consumer thread pops results from the output queue, verifies each reply, and counts what it saw. It never touches a
// driver object.
class consumer {
   public:
    explicit consumer(queue<result>& output) : _output{output} {}

    // The loop of a consumer thread: pops results until the queue is closed and drained. Called on a consumer thread.
    // Errors are streamed to stderr; each message is built as one string, so concurrent writes do not interleave.
    void run() {
        while (auto res = _output.pop()) {
            if (!res->error.empty()) {
                ++_failed_count;
                std::cerr << "request " + std::to_string(res->id) + " failed: " + res->error << '\n';
                continue;
            }

            std::uint32_t doc_count = 0;
            try {
                doc_count = check_find_reply(res->reply, res->group, res->expected_doc_count, k_batch_size);
            } catch (std::exception const& exc) {
                ++_verify_failure_count;
                std::cerr << "request " + std::to_string(res->id) + " failed verification: " + exc.what() << '\n';
                continue;
            }

            ++_ok_count;
            _docs_returned_count += doc_count;
        }
    }

    std::uint64_t get_completed_count() const {
        return _ok_count + _failed_count + _verify_failure_count;
    }

    std::uint64_t get_ok_count() const {
        return _ok_count;
    }

    std::uint64_t get_failed_count() const {
        return _failed_count;
    }

    std::uint64_t get_verify_failure_count() const {
        return _verify_failure_count;
    }

    std::uint64_t get_docs_returned_count() const {
        return _docs_returned_count;
    }

   private:
    queue<result>& _output;

    std::uint64_t _ok_count = 0;
    std::uint64_t _failed_count = 0;
    std::uint64_t _verify_failure_count = 0;
    std::uint64_t _docs_returned_count = 0;
};

// ---------------------------------------------------------------------------------------------
// Connection string

// The connection string may be overridden with the `MONGODB_URI` environment variable, matching the other examples in
// this repository. All other parameters are hardcoded (see Constants).
std::string get_uri() {
    auto const* const uri = getenv("MONGODB_URI");
    return (uri && *uri) ? uri : "mongodb://localhost:27017";
}

} // namespace

int main() {
    auto const uri = get_uri();

    try {
        // One worker thread with its own client, runtime, connection pool, and queues. To scale further, run more
        // workers, each with its own client: only one thread can drive a given runtime at a time.
        worker worker{uri};

        populate_collection(worker.get_database());

        std::cout << "uri:                 " << uri << '\n';
        std::cout << "namespace:           " << k_database << '.' << k_collection << '\n';
        std::cout << "documents:           " << k_doc_count << " (cleared and re-populated)\n";
        std::cout << "max in flight:       " << k_max_in_flight << '\n';
        std::cout << "wait slice:          " << k_wait_slice_ms << " ms\n";
        std::cout << "find batch size:     " << k_batch_size << " (singleBatch: true; first batch only)\n\n";

        auto const start_time = std::chrono::steady_clock::now();

        std::vector<producer> producers;
        producers.reserve(k_producers);
        for (std::uint32_t i = 0; i < k_producers; ++i) {
            producers.emplace_back(worker.get_database(), worker.get_input());
        }

        std::vector<consumer> consumers;
        consumers.reserve(k_consumers);
        for (std::uint32_t i = 0; i < k_consumers; ++i) {
            consumers.emplace_back(worker.get_output());
        }

        std::vector<std::thread> producer_threads;
        producer_threads.reserve(k_producers);
        for (auto& producer : producers) {
            producer_threads.emplace_back(&producer::run, &producer);
        }

        std::vector<std::thread> consumer_threads;
        consumer_threads.reserve(k_consumers);
        for (auto& consumer : consumers) {
            consumer_threads.emplace_back(&consumer::run, &consumer);
        }

        for (auto& thread : producer_threads) {
            thread.join();
        }

        // Stop the worker: it closes its input queue, joins its thread (draining the in-flight operations), closes the
        // output queue, and tears the driver objects down. This is what lets the consumers finish.
        worker.shutdown();

        for (auto& thread : consumer_threads) {
            thread.join();
        }

        auto const elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time);

        // Total the counts internal to each producer and consumer.
        std::uint64_t produced_count = 0;
        std::uint64_t completed_count = 0;
        std::uint64_t ok_count = 0;
        std::uint64_t failed_count = 0;
        std::uint64_t verify_failure_count = 0;
        std::uint64_t docs_returned_count = 0;

        for (auto const& producer : producers) {
            produced_count += producer.get_produced_count();
        }
        for (auto const& consumer : consumers) {
            completed_count += consumer.get_completed_count();
            ok_count += consumer.get_ok_count();
            failed_count += consumer.get_failed_count();
            verify_failure_count += consumer.get_verify_failure_count();
            docs_returned_count += consumer.get_docs_returned_count();
        }

        std::cout << "summary:\n";
        std::cout << "  requests produced:     " << produced_count << '\n';
        std::cout << "  requests completed:    " << completed_count << '\n';
        std::cout << "  replies ok:            " << ok_count << '\n';
        std::cout << "  replies failed:        " << failed_count << '\n';
        std::cout << "  verification failures: " << verify_failure_count << '\n';
        std::cout << "  documents returned:    " << docs_returned_count << '\n';
        std::cout << "  elapsed:               " << std::fixed << std::setprecision(2) << elapsed.count() << " s\n";

        // Report a worker failure before declaring the run failed, so that the root cause is visible: a worker failure
        // also shows up as `completed != k_requests` below.
        if (!worker.get_failure().empty()) {
            std::cerr << "worker failed: " << worker.get_failure() << '\n';
            return EXIT_FAILURE;
        }

        if (failed_count > 0 || verify_failure_count > 0 || completed_count != k_requests) {
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } catch (std::exception const& exc) {
        // Unwinding has already stopped and joined the worker: its destructor calls shutdown().
        std::cerr << "error: " << exc.what() << '\n';
        return EXIT_FAILURE;
    }
}
