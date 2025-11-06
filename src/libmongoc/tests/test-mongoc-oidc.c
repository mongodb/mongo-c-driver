/*
 * Copyright 2009-present MongoDB, Inc.
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

#include <mongoc/mongoc-counters-private.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

static char *
read_test_token(void)
{
   FILE *token_file = fopen("/tmp/tokens/test_machine", "r");
   ASSERT(token_file);

   // Determine length of token:
   ASSERT(0 == fseek(token_file, 0, SEEK_END));
   long token_len = ftell(token_file);
   ASSERT(token_len > 0);
   ASSERT(0 == fseek(token_file, 0, SEEK_SET));

   // Read file into buffer:
   char *token = bson_malloc(token_len + 1);
   size_t nread = fread(token, 1, token_len, token_file);
   ASSERT(nread == (size_t)token_len);
   token[token_len] = '\0';
   fclose(token_file);
   return token;
}

typedef struct {
   bool validate_params;
   bool return_null;
   bool return_bad_token;
   bool return_bad_token_after_first_call;
} callback_config_t;

typedef struct {
   int call_count;
   callback_config_t config;
} callback_ctx_t;

static mongoc_oidc_credential_t *
oidc_callback_fn(mongoc_oidc_callback_params_t *params)
{
   callback_ctx_t *ctx = mongoc_oidc_callback_params_get_user_data(params);
   ASSERT(ctx);
   ctx->call_count += 1;

   if (ctx->config.return_null) {
      return NULL;
   }

   if (ctx->config.return_bad_token) {
      return mongoc_oidc_credential_new("bad_token");
   }

   if (ctx->config.return_bad_token_after_first_call && ctx->call_count > 1) {
      return mongoc_oidc_credential_new("bad_token");
   }

   if (ctx->config.validate_params) {
      const int64_t *timeout = mongoc_oidc_callback_params_get_timeout(params);
      ASSERT(timeout);
      // Expect the timeout to be set to 60 seconds from the start.
      ASSERT_CMPINT64(*timeout, >=, bson_get_monotonic_time());
      ASSERT_CMPINT64(*timeout, <=, bson_get_monotonic_time() + 60 * 1000 * 1000);

      int version = mongoc_oidc_callback_params_get_version(params);
      ASSERT_CMPINT(version, ==, 1);

      const char *username = mongoc_oidc_callback_params_get_username(params);
      ASSERT(!username);
   }

   char *token = read_test_token();
   mongoc_oidc_credential_t *cred = mongoc_oidc_credential_new(token);
   bson_free(token);
   return cred;
}


typedef struct {
   mongoc_client_pool_t *pool; // May be NULL.
   mongoc_client_t *client;
   callback_ctx_t ctx;
} test_fixture_t;

typedef struct {
   bool use_pool;
   bool use_error_api_v1;
   callback_config_t callback_config;
} test_config_t;

static test_fixture_t *
test_fixture_new(test_config_t cfg)
{
   test_fixture_t *tf = bson_malloc0(sizeof(*tf));

   mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost:27017"); // Direct connect for simpler op counters.
   mongoc_uri_set_auth_mechanism(uri, "MONGODB-OIDC");
   mongoc_uri_set_option_as_bool(uri, MONGOC_URI_RETRYREADS, false); // Disable retryable reads per spec.

   mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new(oidc_callback_fn);
   tf->ctx.config = cfg.callback_config;
   mongoc_oidc_callback_set_user_data(oidc_callback, &tf->ctx);

   if (cfg.use_pool) {
      tf->pool = mongoc_client_pool_new(uri);
      mongoc_client_pool_set_error_api(tf->pool, MONGOC_ERROR_API_VERSION_2);
      ASSERT(mongoc_client_pool_set_oidc_callback(tf->pool, oidc_callback));
      tf->client = mongoc_client_pool_pop(tf->pool);
   } else {
      tf->client = mongoc_client_new_from_uri(uri);
      mongoc_client_set_error_api(tf->client, MONGOC_ERROR_API_VERSION_2);
      ASSERT(mongoc_client_set_oidc_callback(tf->client, oidc_callback));
   }

   mongoc_oidc_callback_destroy(oidc_callback);
   mongoc_uri_destroy(uri);
   return tf;
}

static void
test_fixture_destroy(test_fixture_t *tf)
{
   if (!tf) {
      return;
   }
   if (tf->pool) {
      mongoc_client_pool_push(tf->pool, tf->client);
      mongoc_client_pool_destroy(tf->pool);
   } else {
      mongoc_client_destroy(tf->client);
   }
   bson_free(tf);
}

static bool
do_find(mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   bool ret = false;
   bson_t filter = BSON_INITIALIZER;

   coll = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find_with_opts(coll, &filter, NULL, NULL);

   const bson_t *doc;
   while (mongoc_cursor_next(cursor, &doc))
      ;

   if (mongoc_cursor_error(cursor, error)) {
      goto fail;
   }

   ret = true;
fail:
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(coll);
   return ret;
}

static void
configure_failpoint(const char *failpoint_json)
// Configure failpoint on a separate client:
{
   bson_error_t error;
   mongoc_client_t *client = test_framework_new_default_client();

   // Configure fail point:
   bson_t *failpoint = tmp_bson(failpoint_json);

   ASSERT_OR_PRINT(mongoc_client_command_simple(client, "admin", failpoint, NULL, NULL, &error), error);

   mongoc_client_destroy(client);
}

// test_oidc_works tests a simple happy path.
static void
test_oidc_works(void *use_pool_void)
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Expect callback not-yet called:
   ASSERT_CMPINT(tf->ctx.call_count, ==, 0);

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called:
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

// test_oidc_bad_config tests MONGODB-OIDC with bad configurations.
static void
test_oidc_bad_config(void *unused)
{
   bson_error_t error;

   // Expect error is single-threaded setter used on pooled client:
   {
      mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_oidc_callback_t *cb = mongoc_oidc_callback_new(oidc_callback_fn);
      capture_logs(true);
      ASSERT(!mongoc_client_set_oidc_callback(client, cb));
      ASSERT_CAPTURED_LOG("oidc", MONGOC_LOG_LEVEL_ERROR, "only be used for single threaded clients");
      mongoc_oidc_callback_destroy(cb);
      mongoc_client_pool_push(pool, client);
      mongoc_client_pool_destroy(pool);
      mongoc_uri_destroy(uri);
   }

   // Expect error if pool setter used after client is popped:
   {
      mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      mongoc_client_pool_t *pool = mongoc_client_pool_new(uri);
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      mongoc_oidc_callback_t *cb = mongoc_oidc_callback_new(oidc_callback_fn);
      capture_logs(true);
      ASSERT(!mongoc_client_pool_set_oidc_callback(pool, cb));
      ASSERT_CAPTURED_LOG("oidc", MONGOC_LOG_LEVEL_ERROR, "only be called before mongoc_client_pool_pop");
      mongoc_oidc_callback_destroy(cb);
      mongoc_client_pool_push(pool, client);
      mongoc_client_pool_destroy(pool);
      mongoc_uri_destroy(uri);
   }

   // Expect error if no callback set:
   {
      mongoc_client_t *client = mongoc_client_new("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      bool ok = mongoc_client_command_simple(client, "db", tmp_bson("{'ping': 1}"), NULL, NULL, &error);
      ASSERT(!ok);
      ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "no callback set");
      mongoc_client_destroy(client);
   }

   // Expect error if callback is set twice:
   {
      mongoc_client_t *client = mongoc_client_new("mongodb://localhost/?authMechanism=MONGODB-OIDC");
      mongoc_oidc_callback_t *cb = mongoc_oidc_callback_new(oidc_callback_fn);
      ASSERT(mongoc_client_set_oidc_callback(client, cb));
      capture_logs(true);
      ASSERT(!mongoc_client_set_oidc_callback(client, cb));
      ASSERT_CAPTURED_LOG("oidc", MONGOC_LOG_LEVEL_ERROR, "called once");
      mongoc_oidc_callback_destroy(cb);
      mongoc_client_destroy(client);
   }

   // Expect error if callback is set twice on pool:
   {
      mongoc_client_pool_t *pool =
         mongoc_client_pool_new(mongoc_uri_new("mongodb://localhost/?authMechanism=MONGODB-OIDC"));
      mongoc_oidc_callback_t *cb = mongoc_oidc_callback_new(oidc_callback_fn);
      ASSERT(mongoc_client_pool_set_oidc_callback(pool, cb));
      capture_logs(true);
      ASSERT(!mongoc_client_pool_set_oidc_callback(pool, cb));
      ASSERT_CAPTURED_LOG("oidc", MONGOC_LOG_LEVEL_ERROR, "called once");
      mongoc_oidc_callback_destroy(cb);
      mongoc_client_pool_destroy(pool);
   }
}

// test_oidc_delays tests the minimum required time between OIDC calls.
static void
test_oidc_delays(void *use_pool_void)
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Configure failpoint to return ReauthenticationError (391):
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));

   int64_t start_us = bson_get_monotonic_time();

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);

   int64_t end_us = bson_get_monotonic_time();

   ASSERT_CMPINT64(end_us - start_us, >=, 100 * 1000); // At least 100ms between calls to the callback.

   test_fixture_destroy(tf);
}

// test_oidc_reauth_twice tests a reauth error occurring twice in a row.
static void
test_oidc_reauth_twice(void *use_pool_void)
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Configure failpoint to return ReauthenticationError (391):
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 2},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));

   int64_t start_us = bson_get_monotonic_time();

   // Expect error:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, MONGOC_SERVER_ERR_REAUTHENTICATION_REQUIRED, "failpoint");

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);

   int64_t end_us = bson_get_monotonic_time();

   ASSERT_CMPINT64(end_us - start_us, >=, 100 * 1000); // At least 100ms between calls to the callback.

   test_fixture_destroy(tf);
}

// test_oidc_reauth_error_v1 tests a reauth error using the V1 error API.
static void
test_oidc_reauth_error_v1(void *use_pool_void)
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool, .use_error_api_v1 = true});

   // Configure failpoint to return ReauthenticationError (391):
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));

   int64_t start_us = bson_get_monotonic_time();

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);

   int64_t end_us = bson_get_monotonic_time();

   ASSERT_CMPINT64(end_us - start_us, >=, 100 * 1000); // At least 100ms between calls to the callback.

   test_fixture_destroy(tf);
}

#define PROSE_TEST(maj, min, desc) static void test_oidc_prose_##maj##_##min(void *use_pool_void)

PROSE_TEST(1, 1, "Callback is called during authentication")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

static BSON_THREAD_FUN(do_100_finds, pool_void)
{
   mongoc_client_pool_t *pool = pool_void;
   for (int i = 0; i < 100; i++) {
      mongoc_client_t *client = mongoc_client_pool_pop(pool);
      bson_error_t error;
      bool ok = do_find(client, &error);
      ASSERT_OR_PRINT(ok, error);
      mongoc_client_pool_push(pool, client);
   }
   BSON_THREAD_RETURN;
}

PROSE_TEST(1, 2, "Callback is called once for multiple connections")
{
   BSON_UNUSED(use_pool_void); // Test only runs for pooled.
   bool use_pool = true;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Start 10 threads. Each thread runs 100 find operations:
   bson_thread_t threads[10];
   for (int i = 0; i < 10; i++) {
      ASSERT(0 == mcommon_thread_create(&threads[i], do_100_finds, tf->pool));
   }

   // Wait for threads to finish:
   for (int i = 0; i < 10; i++) {
      mcommon_thread_join(threads[i]);
   }

   // Expect callback was called.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

PROSE_TEST(2, 1, "Valid Callback Inputs")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf =
      test_fixture_new((test_config_t){.use_pool = use_pool, .callback_config = {.validate_params = true}});

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   test_fixture_destroy(tf);
}

PROSE_TEST(2, 2, "OIDC Callback Returns Null")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf =
      test_fixture_new((test_config_t){.use_pool = use_pool, .callback_config = {.return_null = true}});

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "OIDC callback failed");

   test_fixture_destroy(tf);
}

PROSE_TEST(2, 3, "OIDC Callback Returns Missing Data")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){
      .use_pool = use_pool,
      .callback_config = {
         // mongoc_oidc_credential_t cannot be partially created. Instead of "missing" data, return a bad token.
         .return_bad_token = true}});

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

   test_fixture_destroy(tf);
}

PROSE_TEST(2, 4, "Invalid Client Configuration with Callback")
{
   BSON_UNUSED(use_pool_void);

   mongoc_uri_t *uri = mongoc_uri_new("mongodb://localhost:27017");
   mongoc_uri_set_auth_mechanism(uri, "MONGODB-OIDC");
   mongoc_uri_set_mechanism_properties(uri, tmp_bson(BSON_STR({"ENVIRONMENT" : "test"})));

   callback_ctx_t ctx;
   mongoc_oidc_callback_t *oidc_callback = mongoc_oidc_callback_new(oidc_callback_fn);
   mongoc_oidc_callback_set_user_data(oidc_callback, &ctx);

   mongoc_client_t *client = mongoc_client_new_from_uri(uri);
   mongoc_client_set_oidc_callback(client, oidc_callback);

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, "Use one or the other");

   mongoc_client_destroy(client);
   mongoc_oidc_callback_destroy(oidc_callback);
   mongoc_uri_destroy(uri);
}

PROSE_TEST(2, 5, "Invalid Client Configuration with Callback")
{
   BSON_UNUSED(use_pool_void);

   bson_error_t error;
   mongoc_uri_t *uri = mongoc_uri_new_with_error(
      "mongodb://localhost:27017/"
      "?retryReads=false&authMechanism=MONGODB-OIDC&authMechanismProperties=ENVIRONMENT:azure,ALLOWED_HOSTS:",
      &error);
   ASSERT(!uri);
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "Unsupported");
   mongoc_uri_destroy(uri);
}

static void
poison_client_cache(mongoc_client_t *client)
{
   BSON_ASSERT_PARAM(client);
   mongoc_oidc_cache_set_cached_token(client->topology->oidc_cache, "bad_token");
}

PROSE_TEST(3, 1, "Authentication failure with cached tokens fetch a new token and retry auth")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   poison_client_cache(tf->client);

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

PROSE_TEST(3, 2, "Authentication failures without cached tokens return an error")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf =
      test_fixture_new((test_config_t){.use_pool = use_pool, .callback_config = {.return_bad_token = true}});

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

   // Expect callback was called.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

PROSE_TEST(3, 3, "Unexpected error code does not clear the cache")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Configure failpoint:
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["saslStart"], "errorCode" : 20}
   }));

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, 20, "Failing command");

   // Expect callback was called.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   // Expect second attempt succeeds:
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was not called again.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

   test_fixture_destroy(tf);
}

PROSE_TEST(4, 1, "Reauthentication Succeeds")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Configure failpoint:
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));

   // Expect auth to succeed:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find(tf->client, &error), error);

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);

   test_fixture_destroy(tf);
}

PROSE_TEST(4, 2, "Read Commands Fail If Reauthentication Fails")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new(
      (test_config_t){.use_pool = use_pool, .callback_config = {.return_bad_token_after_first_call = true}});

   // Configure failpoint:
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));


   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_find(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);
   test_fixture_destroy(tf);
}

static bool
do_insert(mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = NULL;
   bool ret = false;
   bson_t doc = BSON_INITIALIZER;

   coll = mongoc_client_get_collection(client, "test", "test");
   if (!mongoc_collection_insert_one(coll, &doc, NULL, NULL, error)) {
      goto fail;
   }

   ret = true;
fail:
   mongoc_collection_destroy(coll);
   return ret;
}

PROSE_TEST(4, 3, "Write Commands Fail If Reauthentication Fails")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new(
      (test_config_t){.use_pool = use_pool, .callback_config = {.return_bad_token_after_first_call = true}});

   // Configure failpoint:
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["insert"], "errorCode" : 391}
   }));

   // Expect auth to fail:
   bson_error_t error;
   ASSERT(!do_insert(tf->client, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_SERVER, 18, "Authentication failed");

   // Expect callback was called twice: once for initial auth, once for reauth.
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);
   test_fixture_destroy(tf);
}

// If counters are enabled, define operation count checks:
#ifdef MONGOC_ENABLE_SHM_COUNTERS
#define DECL_OPCOUNT() int32_t opcount = mongoc_counter_op_egress_total_count()
#define ASSERT_OPCOUNT(x) ASSERT_CMPINT32(mongoc_counter_op_egress_total_count(), ==, opcount + x)
#else
#define DECL_OPCOUNT() ((void)0)
#define ASSERT_OPCOUNT(x) ((void)0)
#endif

static void
populate_client_cache(mongoc_client_t *client)
{
   BSON_ASSERT_PARAM(client);
   char *access_token = read_test_token();
   mongoc_oidc_cache_set_cached_token(client->topology->oidc_cache, access_token);
   bson_free(access_token);
}

PROSE_TEST(4, 4, "Speculative Authentication should be ignored on Reauthentication")
{
   BSON_UNUSED(use_pool_void);
   bool use_pool = false; // Only run on single to avoid counters being updated by background threads.
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   bson_error_t error;

   // Populate client cache with a valid access token to enforce speculative authentication:
   populate_client_cache(tf->client);

   // Expect successful auth without sending saslStart:
   {
      DECL_OPCOUNT();

      // Expect auth to succeed:
      ASSERT_OR_PRINT(do_insert(tf->client, &error), error);

      // Expect callback was not called:
      ASSERT_CMPINT(tf->ctx.call_count, ==, 0);

      // Expect two commands sent: hello + insert.
      // Expect saslStart was not sent.
      // TODO(CDRIVER-2669): check command started events instead of counters.
      ASSERT_OPCOUNT(2);
   }

   // Expect successful reauth with sending saslStart:
   {
      // Configure failpoint:
      configure_failpoint(BSON_STR({
         "configureFailPoint" : "failCommand",
         "mode" : {"times" : 1},
         "data" : {"failCommands" : ["insert"], "errorCode" : 391}
      }));

      DECL_OPCOUNT();

      // Expect auth to succeed (after reauth):
      ASSERT_OR_PRINT(do_insert(tf->client, &error), error);

      // Expect callback was called:
      ASSERT_CMPINT(tf->ctx.call_count, ==, 1);

      // Check that three commands were sent: insert (fails) + saslStart + insert (succeeds).
      // TODO(CDRIVER-2669): check command started events instead.
      ASSERT_OPCOUNT(3);
   }

   test_fixture_destroy(tf);
}

static bool
do_find_with_session(mongoc_client_t *client, bson_error_t *error)
{
   mongoc_collection_t *coll = NULL;
   mongoc_cursor_t *cursor = NULL;
   bool ret = false;
   bson_t filter = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   mongoc_client_session_t *sess = NULL;

   // Create session:
   sess = mongoc_client_start_session(client, NULL, error);
   if (!sess) {
      goto fail;
   }

   if (!mongoc_client_session_append(sess, &opts, error)) {
      goto fail;
   }

   coll = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find_with_opts(coll, &filter, &opts, NULL);

   const bson_t *doc;
   while (mongoc_cursor_next(cursor, &doc))
      ;

   if (mongoc_cursor_error(cursor, error)) {
      goto fail;
   }

   ret = true;
fail:
   mongoc_client_session_destroy(sess);
   bson_destroy(&opts);
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(coll);
   return ret;
}

PROSE_TEST(4, 5, "Reauthentication Succeeds when a Session is involved")
{
   bool use_pool = *(bool *)use_pool_void;
   test_fixture_t *tf = test_fixture_new((test_config_t){.use_pool = use_pool});

   // Configure failpoint:
   configure_failpoint(BSON_STR({
      "configureFailPoint" : "failCommand",
      "mode" : {"times" : 1},
      "data" : {"failCommands" : ["find"], "errorCode" : 391}
   }));

   // Expect find on a session succeeds:
   bson_error_t error;
   ASSERT_OR_PRINT(do_find_with_session(tf->client, &error), error);

   // Expect callback was called twice:
   ASSERT_CMPINT(tf->ctx.call_count, ==, 2);

   test_fixture_destroy(tf);
}

static int
skip_if_no_oidc(void)
{
   return test_framework_is_oidc() ? 1 : 0;
}

void
test_oidc_auth_install(TestSuite *suite)
{
   static bool single = false;
   static bool pooled = true;

   TestSuite_AddFull(suite, "/oidc/bad_config", test_oidc_bad_config, NULL, NULL, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/works/single", test_oidc_works, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/works/pooled", test_oidc_works, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/delays/single", test_oidc_delays, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/delays/pooled", test_oidc_delays, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/reauth_twice/single", test_oidc_reauth_twice, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/reauth_twice/pooled", test_oidc_reauth_twice, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/reauth_error_v1/single", test_oidc_reauth_error_v1, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/reauth_error_v1/pooled", test_oidc_reauth_error_v1, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/1.1/single", test_oidc_prose_1_1, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/1.1/pooled", test_oidc_prose_1_1, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/1.2", test_oidc_prose_1_2, NULL, NULL, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/2.1/single", test_oidc_prose_2_1, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/2.1/pooled", test_oidc_prose_2_1, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/2.2/single", test_oidc_prose_2_2, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/2.2/pooled", test_oidc_prose_2_2, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/2.3/single", test_oidc_prose_2_3, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/2.3/pooled", test_oidc_prose_2_3, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/2.4", test_oidc_prose_2_4, NULL, NULL, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/2.5", test_oidc_prose_2_5, NULL, NULL, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/3.1/single", test_oidc_prose_3_1, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/3.1/pooled", test_oidc_prose_3_1, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/3.2/single", test_oidc_prose_3_2, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/3.2/pooled", test_oidc_prose_3_2, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/3.3/single", test_oidc_prose_3_3, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/3.3/pooled", test_oidc_prose_3_3, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/4.1/single", test_oidc_prose_4_1, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/4.1/pooled", test_oidc_prose_4_1, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/4.2/single", test_oidc_prose_4_2, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/4.2/pooled", test_oidc_prose_4_2, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/4.3/single", test_oidc_prose_4_3, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/4.3/pooled", test_oidc_prose_4_3, NULL, &pooled, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/4.4", test_oidc_prose_4_4, NULL, NULL, skip_if_no_oidc);

   TestSuite_AddFull(suite, "/oidc/prose/4.5/single", test_oidc_prose_4_5, NULL, &single, skip_if_no_oidc);
   TestSuite_AddFull(suite, "/oidc/prose/4.5/pooled", test_oidc_prose_4_5, NULL, &pooled, skip_if_no_oidc);
}
