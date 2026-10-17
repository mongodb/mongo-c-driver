#include <common-macros-private.h> // BEGIN_IGNORE_DEPRECATIONS
#include <mongoc/mongoc-client-pool-private.h>
#include <mongoc/mongoc-client-private.h>
#include <mongoc/mongoc-util-private.h>

#include <mongoc/mongoc.h>

#include <mlib/time_point.h>
#include <mlib/timer.h>

#include <TestSuite.h>
#include <test-libmongoc.h>

#include <stream-tracker.h>


static void
test_mongoc_client_pool_basic(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=1");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);
   client = mongoc_client_pool_pop(pool);
   BSON_ASSERT(client);
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}

static void
test_mongoc_client_pool_max_connecting(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://127.0.0.1/?maxConnecting=5");
   mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
   ASSERT_CMPUINT32(_mongoc_client_pool_get_topology(pool)->max_connecting, ==, 5);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);

   uri = mongoc_uri_new("mongodb://127.0.0.1/");
   pool = mongoc_client_pool_new(uri);
   ASSERT_CMPUINT32(_mongoc_client_pool_get_topology(pool)->max_connecting, ==, 2);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

typedef struct {
   mongoc_topology_t *topology;
   uint32_t server_id;
   bson_mutex_t mutex;
   mongoc_cond_t cond;
   uint32_t attempts;
   uint32_t active;
   uint32_t peak;
   bool release_workers;
} max_connecting_test_state_t;

static BSON_THREAD_FUN(max_connecting_worker, arg)
{
   max_connecting_test_state_t *state = arg;

   bson_mutex_lock(&state->mutex);
   state->attempts++;
   mongoc_cond_broadcast(&state->cond);
   bson_mutex_unlock(&state->mutex);

   bson_error_t error;
   ASSERT_OR_PRINT(_mongoc_topology_connecting_acquire(state->topology, state->server_id, 5000, &error), error);

   bson_mutex_lock(&state->mutex);
   state->active++;
   state->peak = BSON_MAX(state->peak, state->active);
   mongoc_cond_broadcast(&state->cond);
   while (!state->release_workers) {
      mongoc_cond_wait(&state->cond, &state->mutex);
   }
   state->active--;
   bson_mutex_unlock(&state->mutex);

   _mongoc_topology_connecting_release(state->topology, state->server_id);
   BSON_THREAD_RETURN;
}

static void
test_mongoc_client_pool_max_connecting_limits_concurrent_connections(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://127.0.0.1/?maxConnecting=1");
   mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
   max_connecting_test_state_t state = {.topology = _mongoc_client_pool_get_topology(pool), .server_id = 1};
   bson_thread_t threads[4];
   bson_mutex_init(&state.mutex);
   mongoc_cond_init(&state.cond);

   for (size_t i = 0; i < sizeof threads / sizeof threads[0]; i++) {
      ASSERT_CMPINT(mcommon_thread_create(&threads[i], max_connecting_worker, &state), ==, 0);
   }

   bson_mutex_lock(&state.mutex);
   mlib_timer timer = mlib_expires_after(5, s);
   while (state.attempts < sizeof threads / sizeof threads[0] || state.active == 0) {
      ASSERT(!mlib_timer_is_expired(timer));
      mongoc_cond_timedwait(&state.cond, &state.mutex, mlib_milliseconds_count(mlib_timer_remaining(timer)));
   }
   /* Let all workers reach the connection limit while the first connection is held. */
   bson_mutex_unlock(&state.mutex);
   mlib_sleep_for(100, ms);
   bson_mutex_lock(&state.mutex);
   ASSERT_CMPUINT32(state.active, ==, 1);
   ASSERT_CMPUINT32(state.peak, ==, 1);
   state.release_workers = true;
   mongoc_cond_broadcast(&state.cond);
   bson_mutex_unlock(&state.mutex);

   for (size_t i = 0; i < sizeof threads / sizeof threads[0]; i++) {
      ASSERT_CMPINT(mcommon_thread_join(threads[i]), ==, 0);
   }
   ASSERT_CMPUINT32(state.peak, ==, 1);

   mongoc_cond_destroy(&state.cond);
   bson_mutex_destroy(&state.mutex);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}


/* Called with state->mutex held. */
static void
max_connecting_wait_for(max_connecting_test_state_t *state, const uint32_t *counter, uint32_t expected)
{
   const mlib_timer deadline = mlib_expires_after(5, s);
   while (*counter != expected) {
      ASSERT(!mlib_timer_is_expired(deadline));
      mongoc_cond_timedwait(&state->cond, &state->mutex, mlib_milliseconds_count(mlib_timer_remaining(deadline)));
   }
}

static void
test_mongoc_client_pool_max_connecting_wakes_matching_server(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://127.0.0.1/?maxConnecting=1");
   mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
   mongoc_topology_t *topology = _mongoc_client_pool_get_topology(pool);
   max_connecting_test_state_t states[2] = {
      {.topology = topology, .server_id = 2}, {.topology = topology, .server_id = 1}};
   bson_thread_t threads[2];
   bson_error_t error;

   /* Exhaust both servers, then queue the waiter for server 2 first. */
   ASSERT_OR_PRINT(_mongoc_topology_connecting_acquire(topology, 1, 5000, &error), error);
   ASSERT_OR_PRINT(_mongoc_topology_connecting_acquire(topology, 2, 5000, &error), error);
   for (size_t i = 0; i < 2; i++) {
      bson_mutex_init(&states[i].mutex);
      mongoc_cond_init(&states[i].cond);
      ASSERT_CMPINT(mcommon_thread_create(&threads[i], max_connecting_worker, &states[i]), ==, 0);
      bson_mutex_lock(&states[i].mutex);
      max_connecting_wait_for(&states[i], &states[i].attempts, 1);
      bson_mutex_unlock(&states[i].mutex);
      mlib_sleep_for(100, ms);
   }

   /* Server 1 must make progress even though server 2 stays occupied. */
   _mongoc_topology_connecting_release(topology, 1);
   bson_mutex_lock(&states[1].mutex);
   max_connecting_wait_for(&states[1], &states[1].active, 1);
   states[1].release_workers = true;
   mongoc_cond_broadcast(&states[1].cond);
   bson_mutex_unlock(&states[1].mutex);
   bson_mutex_lock(&states[0].mutex);
   ASSERT_CMPUINT32(states[0].active, ==, 0);
   states[0].release_workers = true;
   mongoc_cond_broadcast(&states[0].cond);
   bson_mutex_unlock(&states[0].mutex);
   _mongoc_topology_connecting_release(topology, 2);

   for (size_t i = 0; i < 2; i++) {
      ASSERT_CMPINT(mcommon_thread_join(threads[i]), ==, 0);
      ASSERT_CMPUINT32(states[i].peak, ==, 1);
      mongoc_cond_destroy(&states[i].cond);
      bson_mutex_destroy(&states[i].mutex);
   }
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

static void
test_mongoc_client_pool_max_connecting_timeout(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://127.0.0.1/?maxConnecting=1");
   mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
   mongoc_topology_t *topology = _mongoc_client_pool_get_topology(pool);
   bson_error_t error;
   ASSERT_OR_PRINT(_mongoc_topology_connecting_acquire(topology, 1, 5000, &error), error);
   const int64_t start = bson_get_monotonic_time();
   ASSERT(!_mongoc_topology_connecting_acquire(topology, 1, 50, &error));
   ASSERT(bson_get_monotonic_time() - start >= 49000);
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_NOT_READY, "maxConnecting slot");
   _mongoc_topology_connecting_release(topology, 1);
   /* Timeout must not leak a slot or increment the connection count. */
   ASSERT_OR_PRINT(_mongoc_topology_connecting_acquire(topology, 1, 50, &error), error);
   _mongoc_topology_connecting_release(topology, 1);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

static void
test_mongoc_client_pool_try_pop(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=1");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);
   client = mongoc_client_pool_pop(pool);
   BSON_ASSERT(client);
   BSON_ASSERT(!mongoc_client_pool_try_pop(pool));
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}

static void
test_mongoc_client_pool_pop_timeout(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   int64_t start;
   int64_t duration_usec;

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=1&waitqueuetimeoutms=2000");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);
   client = mongoc_client_pool_pop(pool);
   BSON_ASSERT(client);
   start = bson_get_monotonic_time();
   BSON_ASSERT(!mongoc_client_pool_pop(pool));
   duration_usec = bson_get_monotonic_time() - start;
   /* There is a possibility that the wait is a few milliseconds short.  The
    * assertion is structured like this since the timeout is a rough lower bound
    * and some test environments might slow things down. */
   BSON_ASSERT(duration_usec / 1000 >= 1990);
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}

static void
test_mongoc_client_pool_min_size_zero(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client1;
   mongoc_client_t *client2;
   mongoc_client_t *client3;
   mongoc_client_t *client4;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new(NULL);
   pool = test_framework_client_pool_new_from_uri(uri, NULL);

   client1 = mongoc_client_pool_pop(pool);
   client2 = mongoc_client_pool_pop(pool);
   mongoc_client_pool_push(pool, client2);
   mongoc_client_pool_push(pool, client1);

   BSON_ASSERT(mongoc_client_pool_get_size(pool) == 2);
   client3 = mongoc_client_pool_pop(pool);

   /* min pool size zero means "no min", so clients weren't destroyed */
   BSON_ASSERT(client3 == client1);
   client4 = mongoc_client_pool_pop(pool);
   BSON_ASSERT(client4 == client2);

   mongoc_client_pool_push(pool, client4);
   mongoc_client_pool_push(pool, client3);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

static void
test_mongoc_client_pool_set_max_size(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   mongoc_array_t conns;
   int i;

   _mongoc_array_init(&conns, sizeof client);

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=10");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);

   for (i = 0; i < 5; i++) {
      client = mongoc_client_pool_pop(pool);
      BSON_ASSERT(client);
      _mongoc_array_append_val(&conns, client);
      BSON_ASSERT(mlib_cmp(mongoc_client_pool_get_size(pool), ==, i + 1));
   }

   mongoc_client_pool_max_size(pool, 3);

   BSON_ASSERT(mongoc_client_pool_try_pop(pool) == NULL);

   for (i = 0; i < 5; i++) {
      client = _mongoc_array_index(&conns, mongoc_client_t *, i);
      BSON_ASSERT(client);
      mongoc_client_pool_push(pool, client);
   }

   _mongoc_array_clear(&conns);
   _mongoc_array_destroy(&conns);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}

typedef struct {
   mongoc_client_pool_t *pool;
   bson_mutex_t mutex;
   mongoc_cond_t cond;
   int started;
   int completed;
   mongoc_client_t *clients[3];
} pool_resize_state_t;

static BSON_THREAD_FUN(pool_resize_worker, arg)
{
   pool_resize_state_t *state = arg;
   bson_mutex_lock(&state->mutex);
   ++state->started;
   mongoc_cond_broadcast(&state->cond);
   bson_mutex_unlock(&state->mutex);

   mongoc_client_t *client = mongoc_client_pool_pop(state->pool);
   BSON_ASSERT(client);

   bson_mutex_lock(&state->mutex);
   state->clients[state->completed++] = client;
   mongoc_cond_broadcast(&state->cond);
   bson_mutex_unlock(&state->mutex);
   BSON_THREAD_RETURN;
}

/* Called with state->mutex locked. */
static bool
pool_resize_wait(pool_resize_state_t *state, const int *counter, int expected, int64_t timeout_ms)
{
   mlib_timer timer = mlib_expires_after(timeout_ms, ms);
   while (*counter != expected) {
      if (mlib_timer_is_expired(timer)) {
         return false;
      }
      mongoc_cond_timedwait(&state->cond, &state->mutex, mlib_milliseconds_count(mlib_timer_remaining(timer)));
   }
   return true;
}

static void
test_mongoc_client_pool_max_size_wakes_waiters(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://127.0.0.1/?maxPoolSize=1");
   mongoc_client_pool_t *pool = test_framework_client_pool_new_from_uri(uri, NULL);
   mongoc_client_t *first = mongoc_client_pool_pop(pool);
   BSON_ASSERT(first);
   pool_resize_state_t state = {.pool = pool};
   bson_thread_t threads[3];
   bson_mutex_init(&state.mutex);
   mongoc_cond_init(&state.cond);

   for (int i = 0; i < 3; ++i) {
      ASSERT_CMPINT(mcommon_thread_create(&threads[i], pool_resize_worker, &state), ==, 0);
   }
   bson_mutex_lock(&state.mutex);
   ASSERT(pool_resize_wait(&state, &state.started, 3, 5000));
   /* Give all started threads time to enter the exhausted pool's wait. */
   ASSERT(!pool_resize_wait(&state, &state.completed, 1, 100));
   bson_mutex_unlock(&state.mutex);

   mongoc_client_pool_max_size(pool, 4);

   /* All waiters must obtain new clients without returning the original client. */
   bson_mutex_lock(&state.mutex);
   ASSERT(pool_resize_wait(&state, &state.completed, 3, 5000));
   bson_mutex_unlock(&state.mutex);
   for (int i = 0; i < 3; ++i) {
      ASSERT_CMPINT(mcommon_thread_join(threads[i]), ==, 0);
      mongoc_client_pool_push(pool, state.clients[i]);
   }
   mongoc_client_pool_push(pool, first);
   mongoc_cond_destroy(&state.cond);
   bson_mutex_destroy(&state.mutex);
   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

#ifndef MONGOC_ENABLE_SSL
static void
test_mongoc_client_pool_ssl_disabled(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://host/?ssl=true");

   ASSERT(uri);
   capture_logs(true);
   ASSERT(NULL == test_framework_client_pool_new_from_uri(uri, NULL));
   ASSERT_CAPTURED_LOG("mongoc_client_pool_new", MONGOC_LOG_LEVEL_ERROR, "SSL not enabled in this build.");

   mongoc_uri_destroy(uri);
}
#endif

static void
test_mongoc_client_pool_handshake(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=1");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);


   ASSERT(mongoc_client_pool_set_appname(pool, "some application"));
   /* Be sure we can't set it twice */
   capture_logs(true);
   ASSERT(!mongoc_client_pool_set_appname(pool, "a"));
   ASSERT_CAPTURED_LOG(
      "_mongoc_topology_scanner_set_appname", MONGOC_LOG_LEVEL_ERROR, "Cannot set appname more than once");
   capture_logs(false);

   mongoc_client_pool_destroy(pool);

   /* Make sure that after we pop a client we can't set handshake anymore */
   pool = test_framework_client_pool_new_from_uri(uri, NULL);

   client = mongoc_client_pool_pop(pool);

   /* Be sure a client can't set it now that we've popped them */
   capture_logs(true);
   ASSERT(!mongoc_client_set_appname(client, "a"));
   ASSERT_CAPTURED_LOG("_mongoc_topology_scanner_set_appname",
                       MONGOC_LOG_LEVEL_ERROR,
                       "Cannot call set_appname on a client from a pool");
   capture_logs(false);

   mongoc_client_pool_push(pool, client);

   /* even now that we pushed the client back we shouldn't be able to set
    * the handshake */
   capture_logs(true);
   ASSERT(!mongoc_client_pool_set_appname(pool, "a"));
   ASSERT_CAPTURED_LOG(
      "_mongoc_topology_scanner_set_appname", MONGOC_LOG_LEVEL_ERROR, "Cannot set appname after handshake initiated");
   capture_logs(false);

   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}

/* Test that destroying a pool without pushing all clients is ok. */
static void
test_client_pool_destroy_without_pushing(void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client_in_pool;
   mongoc_client_t *client1;
   mongoc_client_t *client2;
   bson_error_t error;
   bson_t *cmd;
   bool ret;

   cmd = BCON_NEW("ping", BCON_INT32(1));
   pool = test_framework_new_default_client_pool();
   client1 = mongoc_client_pool_pop(pool);
   client2 = mongoc_client_pool_pop(pool);

   /* Push a client back onto the pool so endSessions succeeds to avoid a
    * warning. */
   client_in_pool = mongoc_client_pool_pop(pool);
   ret = mongoc_client_command_simple(client_in_pool, "admin", cmd, NULL /* read prefs */, NULL /* reply */, &error);
   ASSERT_OR_PRINT(ret, error);
   mongoc_client_pool_push(pool, client_in_pool);


   ret = mongoc_client_command_simple(client1, "admin", cmd, NULL /* read prefs */, NULL /* reply */, &error);
   ASSERT_OR_PRINT(ret, error);
   ret = mongoc_client_command_simple(client2, "admin", cmd, NULL /* read prefs */, NULL /* reply */, &error);
   ASSERT_OR_PRINT(ret, error);

   /* Since clients are checked out of pool, it is technically ok to
    * mongoc_client_destroy them instead of pushing. */
   mongoc_client_destroy(client1);

   /* An operation on client2 should still be ok. */
   ret = mongoc_client_command_simple(client2, "admin", cmd, NULL /* read prefs */, NULL /* reply */, &error);
   ASSERT_OR_PRINT(ret, error);
   mongoc_client_destroy(client2);

   /* Destroy the pool, which destroys the shared topology object. */
   mongoc_client_pool_destroy(pool);

   bson_destroy(cmd);
}

static void
command_started_cb(const mongoc_apm_command_started_t *event)
{
   if (strcmp(mongoc_apm_command_started_get_command_name(event), "endSessions") != 0) {
      return;
   }

   int *const count = (int *)mongoc_apm_command_started_get_context(event);
   (*count)++;
}

/* tests that creating and destroying an unused session
 * in pooled mode does not result in an error log */
static void
test_client_pool_create_unused_session(void *context)
{
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;
   mongoc_client_session_t *session;
   mongoc_apm_callbacks_t *callbacks;
   bson_error_t error;
   int count = 0;

   BSON_UNUSED(context);

   capture_logs(true);

   callbacks = mongoc_apm_callbacks_new();
   pool = test_framework_new_default_client_pool();
   mongoc_apm_set_command_started_cb(callbacks, command_started_cb);
   mongoc_client_pool_set_apm_callbacks(pool, callbacks, &count);

   client = mongoc_client_pool_pop(pool);
   session = mongoc_client_start_session(client, NULL, &error);

   mongoc_client_session_destroy(session);
   mongoc_client_pool_push(pool, client);
   mongoc_client_pool_destroy(pool);
   mongoc_apm_callbacks_destroy(callbacks);
   ASSERT_CMPINT(count, ==, 0);
   ASSERT_NO_CAPTURED_LOGS("mongoc_client_pool_destroy");
}


/* Tests case where thread is blocked waiting for a client to be pushed back
 * into the client pool.  Specifically this tests that the program terminates.
 * Addresses CDRIVER-3757 */
typedef struct pool_timeout {
   mongoc_client_pool_t *pool;
   bson_mutex_t mutex;
   mongoc_cond_t cond;
   int nleft;
} pool_timeout_args_t;

static BSON_THREAD_FUN(worker, arg)
{
   pool_timeout_args_t *args = arg;
   mongoc_client_t *client = mongoc_client_pool_pop(args->pool);
   BSON_ASSERT(client);
   mlib_sleep_for(10, us);
   mongoc_client_pool_push(args->pool, client);
   bson_mutex_lock(&args->mutex);
   /* notify main thread that current thread has terminated */
   args->nleft--;
   mongoc_cond_signal(&args->cond);
   bson_mutex_unlock(&args->mutex);
   BSON_THREAD_RETURN;
}

static void
test_client_pool_max_pool_size_exceeded(void)
{
   mongoc_client_pool_t *pool;
   mongoc_uri_t *uri;
   bson_thread_t thread1, thread2;
   pool_timeout_args_t *args = bson_malloc0(sizeof(pool_timeout_args_t));
   int wait_time = 4000; /* 4000 msec = 4 sec */
   int ret;

   uri = mongoc_uri_new("mongodb://127.0.0.1/?maxpoolsize=1");
   pool = test_framework_client_pool_new_from_uri(uri, NULL);
   args->pool = pool;
   args->nleft = 2;
   bson_mutex_init(&args->mutex);
   mongoc_cond_init(&args->cond);

   ASSERT_CMPINT(0, ==, mcommon_thread_create(&thread1, worker, args));
   ASSERT_CMPINT(0, ==, mcommon_thread_create(&thread2, worker, args));

   bson_mutex_lock(&args->mutex);
   while (args->nleft > 0) {
      ret = mongoc_cond_timedwait(&args->cond, &args->mutex, wait_time);
      /* ret non-zero indicates an error (a timeout) */
      BSON_ASSERT(!ret);
   }
   bson_mutex_unlock(&args->mutex);

   mcommon_thread_join(thread1);
   mcommon_thread_join(thread2);

   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
   bson_free(args);
}

static void
test_client_pool_can_override_sockettimeoutms(void)
{
   mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost:27017/?socketTimeoutMS=1000");
   mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);

   // Override the client's socketTimeoutMS.
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      ASSERT_CMPINT32(client->cluster.sockettimeoutms, ==, 1000);
      mongoc_client_set_sockettimeoutms(client, 2000);
      ASSERT_CMPINT32(client->cluster.sockettimeoutms, ==, 2000);
      mongoc_client_pool_push(pool, client);
   }

   // Pop again. Expect the newly popped client to have the socketTimeoutMS from the URI.
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      ASSERT_CMPINT32(client->cluster.sockettimeoutms, ==, 1000);
      mongoc_client_pool_push(pool, client);
   }

   mongoc_client_pool_destroy(pool);
   mongoc_uri_destroy(uri);
}

// Test connections to removed servers are closed when a client is pushed back to the pool.
static void
disconnects_removed_servers_on_push(void *unused)
{
   BSON_UNUSED(unused);
   bson_error_t error;
   bool ok;
   bson_t *ping = BCON_NEW("ping", BCON_INT32(1));

   const char *host0 = "localhost:27017";
   const char *host1 = "localhost:27018";
   stream_tracker_t *st = stream_tracker_new();

   // Create a client pool to two servers.
   mongoc_client_pool_t *pool;
   {
      char *uristr = bson_strdup_printf("mongodb://%s,%s", host0, host1);
      mongoc_uri_t *uri = mongoc_uri_new(uristr);
      // Set a short heartbeat so server monitors get quick responses.
      mongoc_uri_set_option_as_int32(uri, MONGOC_URI_HEARTBEATFREQUENCYMS, MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS);
      pool = mongoc_client_pool_new(uri);
      test_framework_set_pool_ssl_opts(pool);
      mongoc_uri_destroy(uri);
      bson_free(uristr);
   }

   stream_tracker_track_pool(st, pool);

   // Expect no streams created yet:
   stream_tracker_assert_active_count(st, host0, 0);
   stream_tracker_assert_active_count(st, host1, 0);

   // Pop (and push) a client to start background monitoring.
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
      // Wait for monitoring connections to be created.
      // Expect two monitoring connections per server to be created in background.
      stream_tracker_assert_eventual_active_count(st, host0, 2);
      stream_tracker_assert_eventual_active_count(st, host1, 2);
   }

   // Send 'ping' commands on a client to each server to create operation connections.
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      ok = mongoc_client_command_simple_with_server_id(client, "admin", ping, NULL, 1 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);
      ok = mongoc_client_command_simple_with_server_id(client, "admin", ping, NULL, 2 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);
      mongoc_client_pool_push(pool, client);
      // Expect an operation connection is created.
      stream_tracker_assert_active_count(st, host0, 2 + 1);
      stream_tracker_assert_active_count(st, host1, 2 + 1);
   }

   // Mock removal of server 27018 from topology.
   {
      mongoc_topology_t *tp = _mongoc_client_pool_get_topology(pool);
      mc_tpld_modification tdmod = mc_tpld_modify_begin(tp);
      mongoc_set_rm(mc_tpld_servers(tdmod.new_td), 2 /* server ID */);
      mc_tpld_modify_commit(tdmod);
   }

   // Expect connections are closed to removed server.
   {
      // Expect monitoring connections to be closed in background.
      stream_tracker_assert_eventual_active_count(st, host0, 2 + 1);
      stream_tracker_assert_eventual_active_count(st, host1, 1);

      // Pop and push the client to "prune" the stale operation connections.
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
      stream_tracker_assert_active_count(st, host0, 2 + 1);
      stream_tracker_assert_active_count(st, host1, 0);
   }

   mongoc_client_pool_destroy(pool);
   bson_destroy(ping);
   stream_tracker_destroy(st);
}

// Test that connections are closed to servers removed from the topology on clients checked into the pool.
static void
disconnects_removed_servers_in_pool(void *unused)
{
   BSON_UNUSED(unused);
   bson_error_t error;
   bool ok;
   bson_t *ping = BCON_NEW("ping", BCON_INT32(1));

   const char *host0 = "localhost:27017";
   const char *host1 = "localhost:27018";
   stream_tracker_t *st = stream_tracker_new();

   // Create a client pool to two servers.
   mongoc_client_pool_t *pool;
   {
      char *uristr = bson_strdup_printf("mongodb://%s,%s", host0, host1);
      mongoc_uri_t *uri = mongoc_uri_new(uristr);
      // Set a short heartbeat so server monitors get quick responses.
      mongoc_uri_set_option_as_int32(uri, MONGOC_URI_HEARTBEATFREQUENCYMS, MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS);
      pool = mongoc_client_pool_new(uri);
      test_framework_set_pool_ssl_opts(pool);
      mongoc_uri_destroy(uri);
      bson_free(uristr);
   }

   stream_tracker_track_pool(st, pool);

   // Expect no streams created yet:
   stream_tracker_assert_active_count(st, host0, 0);
   stream_tracker_assert_active_count(st, host1, 0);

   // Pop (and push) a client to start background monitoring.
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
      // Wait for monitoring connections to be created.
      // Expect two monitoring connections per server to be created in background.
      stream_tracker_assert_eventual_active_count(st, host0, 2);
      stream_tracker_assert_eventual_active_count(st, host1, 2);
   }

   // Send 'ping' commands on two clients to each server to create operation connections.
   {
      mongoc_client_t *client1 = mongoc_client_pool_pop(pool);
      mongoc_client_t *client2 = mongoc_client_pool_pop(pool);

      ok = mongoc_client_command_simple_with_server_id(client1, "admin", ping, NULL, 1 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);
      ok = mongoc_client_command_simple_with_server_id(client1, "admin", ping, NULL, 2 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);

      ok = mongoc_client_command_simple_with_server_id(client2, "admin", ping, NULL, 1 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);
      ok = mongoc_client_command_simple_with_server_id(client2, "admin", ping, NULL, 2 /* server ID */, NULL, &error);
      ASSERT_OR_PRINT(ok, error);

      mongoc_client_pool_push(pool, client2);
      mongoc_client_pool_push(pool, client1);

      // Expect an operation connection is created per client.
      stream_tracker_assert_active_count(st, host0, 2 + 2);
      stream_tracker_assert_active_count(st, host1, 2 + 2);
   }

   // Mock removal of server 27018 from topology.
   {
      mongoc_topology_t *tp = _mongoc_client_pool_get_topology(pool);
      mc_tpld_modification tdmod = mc_tpld_modify_begin(tp);
      mongoc_set_rm(mc_tpld_servers(tdmod.new_td), 2 /* server ID */);
      mc_tpld_modify_commit(tdmod);
   }

   // Expect connections are closed to removed server.
   {
      // Expect monitoring connections to be closed in background.
      stream_tracker_assert_eventual_active_count(st, host0, 2 + 2);
      stream_tracker_assert_eventual_active_count(st, host1, 2);

      // Pop and push one client to "prune" the stale operation connections for both clients.
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
      stream_tracker_assert_active_count(st, host0, 2 + 2);
      stream_tracker_assert_active_count(st, host1, 0);
   }

   mongoc_client_pool_destroy(pool);
   stream_tracker_destroy(st);
   bson_destroy(ping);
}

/* Test no memory leaks when changing ssl_opts from re-creating OpenSSL context. */
#if defined(MONGOC_ENABLE_SSL_OPENSSL)
static void
test_mongoc_client_pool_change_openssl_ctx(void)
{
   mongoc_client_pool_t *pool;
   const mongoc_ssl_opt_t *ssl_opts;

   pool = test_framework_new_default_client_pool();

   /* change ssl opts */
   ssl_opts = test_framework_get_ssl_opts();
   mongoc_client_pool_set_ssl_opts(pool, ssl_opts);

   mongoc_client_pool_destroy(pool);
}
#endif // MONGOC_ENABLE_SSL_OPENSSL

#if defined(MONGOC_ENABLE_SSL)
static void
test_mongoc_client_set_ssl_opts_on_pool(void)
{
   // Test calling `mongoc_client_set_ssl_opts` on a pooled client logs an error.
   mongoc_client_pool_t *pool = test_framework_new_default_client_pool();
   mongoc_client_t *client_from_pool = mongoc_client_pool_pop(pool);
   capture_logs(true);
   mongoc_client_set_ssl_opts(client_from_pool, mongoc_ssl_opt_get_default());
   ASSERT_CAPTURED_LOG("mongoc_client_set_ssl_opts", MONGOC_LOG_LEVEL_ERROR, "cannot be called on a pooled client");
   capture_logs(false);
   mongoc_client_pool_push(pool, client_from_pool);
   mongoc_client_pool_destroy(pool);
}
#endif // MONGOC_ENABLE_SSL

static void
test_mongoc_client_set_stream_initiator(void)
{
   // Test calling `mongoc_client_set_initiator` on a pooled client logs an error.
   mongoc_client_pool_t *pool = test_framework_new_default_client_pool();
   mongoc_client_t *client_from_pool = mongoc_client_pool_pop(pool);
   capture_logs(true);
   mongoc_client_set_stream_initiator(client_from_pool, mongoc_client_default_stream_initiator, NULL);
   ASSERT_CAPTURED_LOG(
      "mongoc_client_set_stream_initiator", MONGOC_LOG_LEVEL_ERROR, "cannot be called on a pooled client");
   capture_logs(false);
   mongoc_client_pool_push(pool, client_from_pool);
   mongoc_client_pool_destroy(pool);
}

void
test_client_pool_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/ClientPool/basic", test_mongoc_client_pool_basic);
   TestSuite_Add(suite, "/ClientPool/max_connecting", test_mongoc_client_pool_max_connecting);
   TestSuite_Add(suite, "/ClientPool/max_connecting_timeout", test_mongoc_client_pool_max_connecting_timeout);
   TestSuite_Add(suite,
                 "/ClientPool/max_connecting_wakes_matching_server",
                 test_mongoc_client_pool_max_connecting_wakes_matching_server);
   TestSuite_Add(suite,
                 "/ClientPool/max_connecting_limits_concurrent_connections",
                 test_mongoc_client_pool_max_connecting_limits_concurrent_connections);
   TestSuite_Add(suite, "/ClientPool/try_pop", test_mongoc_client_pool_try_pop);
   TestSuite_Add(suite, "/ClientPool/pop_timeout", test_mongoc_client_pool_pop_timeout);
   TestSuite_Add(suite, "/ClientPool/min_size_zero", test_mongoc_client_pool_min_size_zero);
   TestSuite_Add(suite, "/ClientPool/set_max_size", test_mongoc_client_pool_set_max_size);
   TestSuite_Add(suite, "/ClientPool/max_size_wakes_waiters", test_mongoc_client_pool_max_size_wakes_waiters);

   TestSuite_Add(suite, "/ClientPool/handshake", test_mongoc_client_pool_handshake);

   TestSuite_AddFull(suite,
                     "/ClientPool/create_client_pool_unused_session [lock:live-server]",
                     test_client_pool_create_unused_session,
                     NULL /* dtor */,
                     NULL /* ctx */,
                     test_framework_skip_if_no_sessions);
#ifndef MONGOC_ENABLE_SSL
   TestSuite_Add(suite, "/ClientPool/ssl_disabled", test_mongoc_client_pool_ssl_disabled);
#endif
   TestSuite_AddLive(suite, "/ClientPool/destroy_without_push", test_client_pool_destroy_without_pushing);
   TestSuite_AddLive(suite, "/ClientPool/max_pool_size_exceeded", test_client_pool_max_pool_size_exceeded);
   TestSuite_Add(suite,
                 "/ClientPool/can_override_sockettimeoutms [lock:live-server]",
                 test_client_pool_can_override_sockettimeoutms);

   TestSuite_AddFull(
      suite,
      "/client_pool/disconnects_removed_servers/on_push [lock:live-server]",
      disconnects_removed_servers_on_push,
      NULL,
      NULL,
      test_framework_skip_if_not_mongos /* require mongos to ensure two servers available */,
      test_framework_skip_if_max_wire_version_less_than_9 /* require server 4.4+ for streaming monitoring protocol */);

   TestSuite_AddFull(
      suite,
      "/client_pool/disconnects_removed_servers/in_pool [lock:live-server]",
      disconnects_removed_servers_in_pool,
      NULL,
      NULL,
      test_framework_skip_if_not_mongos /* require mongos to ensure two servers available */,
      test_framework_skip_if_max_wire_version_less_than_9 /* require server 4.4+ for streaming monitoring protocol */);

#if defined(MONGOC_ENABLE_SSL_OPENSSL)
   TestSuite_Add(
      suite, "/ClientPool/openssl/change_ssl_opts [lock:live-server]", test_mongoc_client_pool_change_openssl_ctx);
#endif

#if defined(MONGOC_ENABLE_SSL)
   TestSuite_AddLive(suite, "/ClientPool/mongoc_client_set_ssl_opts", test_mongoc_client_set_ssl_opts_on_pool);
#endif // MONGOC_ENABLE_SSL
   TestSuite_AddLive(suite, "/ClientPool/mongoc_client_set_stream_initiator", test_mongoc_client_set_stream_initiator);
}
