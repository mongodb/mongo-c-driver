/*
 * Copyright 2026-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <common-oid-private.h> // kZeroObjectId
#include <common-thread-private.h>
#include <mongoc/mongoc-client-pool-private.h>

#include <mongoc/mongoc.h>

#include <bson/bson.h>

#include <mlib/time_point.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

// CPB_thread_data_thread_data is the Connection Pool Backpressure thread data.
typedef struct {
   mongoc_client_pool_t *pool;
   int connection_failures;
   bool failed;
   bson_mutex_t mutex;
   bson_t *filter;
} CPB_thread_data;

static CPB_thread_data *
CPB_thread_data_new(mongoc_client_pool_t *pool)
{
   CPB_thread_data *thread_data = bson_malloc0(sizeof(CPB_thread_data));
   thread_data->pool = pool;
   thread_data->filter =
      bson_new_from_json((const uint8_t *)BSON_STR({"$where" : "function() { sleep(2000); return true; }"}), -1, NULL);
   bson_mutex_init(&thread_data->mutex);
   return thread_data;
}

static void
CPB_thread_data_failed(CPB_thread_data *thread_data)
{
   bson_mutex_lock(&thread_data->mutex);
   thread_data->failed = true;
   bson_mutex_unlock(&thread_data->mutex);
}

static bool
CPB_thread_data_get_failed(CPB_thread_data *thread_data)
{
   bool ret;
   bson_mutex_lock(&thread_data->mutex);
   ret = thread_data->failed;
   bson_mutex_unlock(&thread_data->mutex);
   return ret;
}

static int
CPB_thread_data_get_connection_failures(CPB_thread_data *thread_data)
{
   int failures;
   bson_mutex_lock(&thread_data->mutex);
   failures = thread_data->connection_failures;
   bson_mutex_unlock(&thread_data->mutex);
   return failures;
}

static void
CPB_thread_data_increment_connection_failures(CPB_thread_data *thread_data)
{
   bson_mutex_lock(&thread_data->mutex);
   thread_data->connection_failures++;
   bson_mutex_unlock(&thread_data->mutex);
}

static void
CPB_thread_data_new_destroy(CPB_thread_data *thread_data)
{
   if (!thread_data) {
      return;
   }
   bson_destroy(thread_data->filter);
   bson_mutex_destroy(&thread_data->mutex);
   bson_free(thread_data);
}

static void
run_admin_command(const char *cmd_str)
{
   bson_t *cmd_bson = tmp_bson(cmd_str);
   bson_error_t error;
   mongoc_client_t *client = test_framework_new_default_client();
   bool ok = mongoc_client_command_simple(client, "admin", cmd_bson, NULL, NULL, &error);
   ASSERT_OR_PRINT(ok, error);
   mongoc_client_destroy(client);
}

static BSON_THREAD_FUN(Connection_Pool_Backpressure_worker, arg)
{
   CPB_thread_data *thread_data = (CPB_thread_data *)arg;
   mongoc_client_t *client = mongoc_client_pool_pop(thread_data->pool);
   mongoc_collection_t *coll = mongoc_client_get_collection(client, "test", "test");

   mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(coll, thread_data->filter, NULL, NULL);

   const bson_t *got;
   bool found = mongoc_cursor_next(cursor, &got);
   bson_error_t error;
   if (!found) {
      // If no document returned, expect a connection failure:
      if (!mongoc_cursor_error(cursor, &error)) {
         MONGOC_ERROR("Unexpected: no document returned, but no error");
         CPB_thread_data_failed(thread_data);
      } else if (!_mongoc_error_is_network(&error)) {
         MONGOC_ERROR("Unexpected non-network error: %s", error.message);
         CPB_thread_data_failed(thread_data);
      } else {
         // OK: expected error.
         CPB_thread_data_increment_connection_failures(thread_data);
      }
   }

   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(coll);
   mongoc_client_pool_push(thread_data->pool, client);
   BSON_THREAD_RETURN;
}

static uint32_t
get_connection_pool_generation(mongoc_client_pool_t *pool)
{
   mongoc_topology_t *topology = _mongoc_client_pool_get_topology(pool);
   mc_shared_tpld td_ref = mc_tpld_take_ref(topology);
   uint32_t generation = _mongoc_topology_get_connection_pool_generation(td_ref.ptr, 1, &kZeroObjectId);
   mc_tpld_drop_ref(&td_ref);
   return generation;
}

// test_Connection_Pool_Backpressure partially implements the "Connection Pool Backpressure" SDAM prose test.
// Some changes are made since libmongoc does not support CMAP events.
static void
test_Connection_Pool_Backpressure(void *unused)
{
   BSON_UNUSED(unused);
   mongoc_client_pool_t *pool = test_framework_new_default_client_pool();
   CPB_thread_data *thread_data = CPB_thread_data_new(pool);

   // Do NOT assert anything until "ingressConnectionEstablishmentRateLimiterEnabled" is set to `false`.
   bool test_passed = false;

   // Set up the rate limiter:
   {
      run_admin_command(BSON_STR({"setParameter" : 1, "ingressConnectionEstablishmentRateLimiterEnabled" : true}));
      run_admin_command(BSON_STR({"setParameter" : 1, "ingressConnectionEstablishmentRatePerSec" : 20}));
      run_admin_command(BSON_STR({"setParameter" : 1, "ingressConnectionEstablishmentBurstCapacitySecs" : 1}));
      run_admin_command(BSON_STR({"setParameter" : 1, "ingressConnectionEstablishmentMaxQueueDepth" : 1}));
   }

   // Add a document to the test collection so that the sleep operations will actually block:
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "test", "test");
      bson_error_t error;
      mongoc_collection_drop(coll, NULL); // Drop pre-existing data.
      bool ok = mongoc_collection_insert_one(coll, tmp_bson("{}"), NULL, NULL, &error);
      mongoc_collection_destroy(coll);
      mongoc_client_pool_push(pool, client);
      if (!ok) {
         MONGOC_ERROR("Failed to insert: %s", error.message);
         goto fail;
      }
   }


   // Run 100 threads to completion:
   {
      bson_thread_t workers[100];

      for (int i = 0; i < 100; i++) {
         mcommon_thread_create(&workers[i], Connection_Pool_Backpressure_worker, thread_data);
      }

      // Join all threads:
      for (int i = 0; i < 100; i++) {
         mcommon_thread_join(workers[i]);
      }
   }

   if (CPB_thread_data_get_failed(thread_data)) {
      MONGOC_ERROR("One or more worker threads failed unexpectedly.");
      goto fail;
   }

   // Expect at least 10 connection failures due to backpressure:
   if (CPB_thread_data_get_connection_failures(thread_data) < 10) {
      MONGOC_ERROR("Expected at least 10 connection failures due to backpressure, but got %d",
                   CPB_thread_data_get_connection_failures(thread_data));
      goto fail;
   }

   // Expect no pool clears. libmongoc does not implement CMAP events. Instead, check for pool clears by inspecting the
   // generation counter.
   if (get_connection_pool_generation(pool) > 0) {
      MONGOC_ERROR("Expected no pool clears, but generation counter was %u", get_connection_pool_generation(pool));
      goto fail;
   }

   test_passed = true;

fail:
   // Disable rate limiter "even if the test fails":
   {
      // Sleep for 1 second to clear the rate limiter.
      mlib_sleep_for(1, s);
      run_admin_command(BSON_STR({"setParameter" : 1, "ingressConnectionEstablishmentRateLimiterEnabled" : false}));
   }

   if (!test_passed) {
      test_error("Test failed. See logs for details.");
   }

   CPB_thread_data_new_destroy(thread_data);

   mongoc_client_pool_destroy(pool);
}

typedef struct {
   int heartbeat_succeeded;
   int changed_to_primary;
   bson_mutex_t mutex;
} SDAM_event_data;

static SDAM_event_data *
SDAM_event_data_new(void)
{
   SDAM_event_data *event_data = bson_malloc0(sizeof(SDAM_event_data));
   bson_mutex_init(&event_data->mutex);
   return event_data;
}

static int
SDAM_event_data_get_heartbeat_succeeded(SDAM_event_data *event_data)
{
   int heartbeat_succeeded;
   bson_mutex_lock(&event_data->mutex);
   heartbeat_succeeded = event_data->heartbeat_succeeded;
   bson_mutex_unlock(&event_data->mutex);
   return heartbeat_succeeded;
}

static int
SDAM_event_data_get_changed_to_primary(SDAM_event_data *event_data)
{
   int changed_to_primary;
   bson_mutex_lock(&event_data->mutex);
   changed_to_primary = event_data->changed_to_primary;
   bson_mutex_unlock(&event_data->mutex);
   return changed_to_primary;
}

static void
SDAM_event_data_destroy(SDAM_event_data *event_data)
{
   if (!event_data) {
      return;
   }
   bson_mutex_destroy(&event_data->mutex);
   bson_free(event_data);
}

static void
SDAM_event_data_heartbeat_succeeded(const mongoc_apm_server_heartbeat_succeeded_t *event)
{
   SDAM_event_data *event_data = (SDAM_event_data *)mongoc_apm_server_heartbeat_succeeded_get_context(event);
   bson_mutex_lock(&event_data->mutex);
   event_data->heartbeat_succeeded++;
   bson_mutex_unlock(&event_data->mutex);
}

static void
SDAM_event_data_server_changed(const mongoc_apm_server_changed_t *event)
{
   SDAM_event_data *event_data = (SDAM_event_data *)mongoc_apm_server_changed_get_context(event);
   if (mongoc_apm_server_changed_get_new_description(event)->type == MONGOC_SERVER_RS_PRIMARY) {
      bson_mutex_lock(&event_data->mutex);
      event_data->changed_to_primary++;
      bson_mutex_unlock(&event_data->mutex);
   }
}

static void
SDAM_event_data_set_callbacks(SDAM_event_data *event_data, mongoc_client_pool_t *pool)
{
   mongoc_apm_callbacks_t *callbacks = mongoc_apm_callbacks_new();

   mongoc_apm_set_server_heartbeat_succeeded_cb(callbacks, SDAM_event_data_heartbeat_succeeded);
   mongoc_apm_set_server_changed_cb(callbacks, SDAM_event_data_server_changed);
   mongoc_client_pool_set_apm_callbacks(pool, callbacks, event_data);
   mongoc_apm_callbacks_destroy(callbacks);
}

// test_SDAM_backpressure_network_error_fail models spec test: backpressure-network-error-fail.yml
static void
test_SDAM_backpressure_network_error_fail(void *unused)
{
   BSON_UNUSED(unused);

   bool is_replset = test_framework_is_replset();

   // Disable failpoint if enabled from previous test:
   run_admin_command(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : "off",
      "data" : {
         "failCommands" : [ "hello", "isMaster" ],
         "appName" : "backpressureNetworkErrorFailTest",
         "closeConnection" : true
      }
   }));

   mongoc_uri_t *uri = test_framework_get_uri();
   mongoc_uri_set_option_as_bool(uri, MONGOC_URI_RETRYWRITES, false);
   mongoc_uri_set_option_as_int32(uri, MONGOC_URI_HEARTBEATFREQUENCYMS, 1000000);
   mongoc_uri_set_option_as_utf8(uri, MONGOC_URI_SERVERMONITORINGMODE, "poll");
   mongoc_uri_set_appname(uri, "backpressureNetworkErrorFailTest");

   mongoc_client_pool_t *pool = test_framework_client_pool_new_from_uri(uri, NULL);
   test_framework_set_pool_ssl_opts(pool);

   SDAM_event_data *event_data = SDAM_event_data_new();
   SDAM_event_data_set_callbacks(event_data, pool);

   // Pop a client to start background monitoring:
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
   }

   if (!is_replset) {
      // Await the first hello.
      WAIT_UNTIL(SDAM_event_data_get_heartbeat_succeeded(event_data) == 1);
   } else {
      // Await discovery of primary.
      WAIT_UNTIL(SDAM_event_data_get_changed_to_primary(event_data) == 1);
   }

   // Configure failpoint to cause network error on hello:
   run_admin_command(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : "alwaysOn",
      "data" : {
         "failCommands" : [ "hello", "isMaster" ],
         "appName" : "backpressureNetworkErrorFailTest",
         "closeConnection" : true
      }
   }));


   // Insert to trigger error
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "sdam-tests", "backpressure-network-error-fail");

      bson_error_t error;
      bson_t reply;
      bool ok = mongoc_collection_insert_one(coll, tmp_bson("{}"), NULL, &reply, &error);
      ASSERT(!ok);
      ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET, "socket error");
      ASSERT(mongoc_error_has_label(&reply, MONGOC_ERROR_LABEL_SYSTEMOVERLOADEDERROR));
      ASSERT(mongoc_error_has_label(&reply, MONGOC_ERROR_LABEL_RETRYABLEERROR));
      bson_destroy(&reply);
      mongoc_collection_destroy(coll);
      mongoc_client_pool_push(pool, client);
   }

   // Expect no pool clear occurred:
   ASSERT_CMPUINT32(get_connection_pool_generation(pool), ==, 0);

   mongoc_client_pool_destroy(pool);
   SDAM_event_data_destroy(event_data);
   mongoc_uri_destroy(uri);
}

// test_SDAM_backpressure_network_error_fail models spec test: backpressure-network-timeout-fail.yml
static void
test_SDAM_backpressure_network_timeout_fail(void *unused)
{
   BSON_UNUSED(unused);

   bool is_replset = test_framework_is_replset();

   // Disable failpoint if enabled from previous test:
   run_admin_command(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : "off",
      "data" : {
         "failCommands" : [ "hello", "isMaster" ],
         "appName" : "backpressureNetworkTimeoutErrorTest",
         "closeConnection" : true
      }
   }));

   mongoc_uri_t *uri = test_framework_get_uri();
   mongoc_uri_set_option_as_bool(uri, MONGOC_URI_RETRYWRITES, false);
   mongoc_uri_set_option_as_int32(uri, MONGOC_URI_HEARTBEATFREQUENCYMS, 1000000);
   mongoc_uri_set_option_as_utf8(uri, MONGOC_URI_SERVERMONITORINGMODE, "poll");
   mongoc_uri_set_option_as_int32(uri, MONGOC_URI_CONNECTTIMEOUTMS, 250);
   mongoc_uri_set_option_as_int32(uri, MONGOC_URI_SOCKETTIMEOUTMS, 250);
   mongoc_uri_set_appname(uri, "backpressureNetworkTimeoutErrorTest");

   mongoc_client_pool_t *pool = test_framework_client_pool_new_from_uri(uri, NULL);
   test_framework_set_pool_ssl_opts(pool);

   SDAM_event_data *event_data = SDAM_event_data_new();
   SDAM_event_data_set_callbacks(event_data, pool);

   // Pop a client to start background monitoring:
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_client_pool_push(pool, client);
   }

   if (!is_replset) {
      // Await the first hello.
      WAIT_UNTIL(SDAM_event_data_get_heartbeat_succeeded(event_data) == 1);
   } else {
      // Await discovery of primary.
      WAIT_UNTIL(SDAM_event_data_get_changed_to_primary(event_data) == 1);
   }

   // Configure failpoint to cause network error on hello:
   run_admin_command(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : "alwaysOn",
      "data" : {
         "failCommands" : [ "hello", "isMaster" ],
         "appName" : "backpressureNetworkTimeoutErrorTest",
         "blockConnection" : true,
         "blockTimeMS" : 500
      }
   }));

   // Insert to trigger error
   {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_collection_t *coll = mongoc_client_get_collection(client, "sdam-tests", "backpressure-network-error-fail");

      bson_error_t error;
      bson_t reply;
      bool ok = mongoc_collection_insert_one(coll, tmp_bson("{}"), NULL, &reply, &error);
      ASSERT(!ok);
      ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_STREAM, MONGOC_ERROR_STREAM_SOCKET, "socket error");
      ASSERT(mongoc_error_has_label(&reply, "SystemOverloadedError"));
      ASSERT(mongoc_error_has_label(&reply, "RetryableError"));
      bson_destroy(&reply);
      mongoc_collection_destroy(coll);
      mongoc_client_pool_push(pool, client);
   }

   // Expect no pool clear occurred:
   ASSERT_CMPUINT32(get_connection_pool_generation(pool), ==, 0);

   mongoc_client_pool_destroy(pool);
   SDAM_event_data_destroy(event_data);
   mongoc_uri_destroy(uri);
}

void
test_backpressure_install(TestSuite *suite)
{
   TestSuite_AddFull(suite,
                     "/backpressure/Connection_Pool_Backpressure",
                     test_Connection_Pool_Backpressure,
                     NULL,
                     NULL,
                     test_framework_skip_if_max_wire_version_less_than_21, /* Require server 7.0 */
                     test_framework_skip_if_slow /* Does many slow blocking operations */);

   TestSuite_AddFull(suite,
                     "/backpressure/SDAM/backpressure-network-error-fail",
                     test_SDAM_backpressure_network_error_fail,
                     NULL,
                     NULL,
                     test_framework_skip_if_mongos, // Only expected to run on single and replica set.
                     test_framework_skip_if_max_wire_version_less_than_21 /* Require server 7.0 */);


   TestSuite_AddFull(suite,
                     "/backpressure/SDAM/backpressure-network-timeout-fail",
                     test_SDAM_backpressure_network_timeout_fail,
                     NULL,
                     NULL,
                     test_framework_skip_if_mongos, // Only expected to run on single and replica set.
                     test_framework_skip_if_max_wire_version_less_than_21 /* Require server 7.0 */);
}
