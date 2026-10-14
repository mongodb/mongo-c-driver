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

/* Tests for the validation of database and collection name arguments. A "." in a database name and a NUL byte in
 * either name cannot be faithfully forwarded to the server, and are rejected. A "." in a collection name is
 * permitted. See DRIVERS-3600.
 *
 * The handle constructors (`mongoc_client_get_database` and friends) still accept any name: they have no
 * `bson_error_t` out-parameter and have never returned NULL. Rejection happens when an operation is performed. */

#include <mongoc/mongoc.h>

#include <TestSuite.h>
#include <test-conveniences.h>
#include <test-libmongoc.h>

#define BAD_DB "db.bad"
#define EXPECT_BAD_DB(error) \
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_NAMESPACE, MONGOC_ERROR_NAMESPACE_INVALID, "invalid: contains \".\"")

/* A handle for an invalid database name is still created: that behavior is unchanged. */
static void
test_ns_validation_handles_still_created(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();

   mongoc_database_t *const db = mongoc_client_get_database(client, BAD_DB);
   ASSERT(db);
   ASSERT_CMPSTR(mongoc_database_get_name(db), BAD_DB);

   mongoc_collection_t *const coll = mongoc_client_get_collection(client, BAD_DB, "coll");
   ASSERT(coll);

   mongoc_collection_t *const coll2 = mongoc_database_get_collection(db, "coll");
   ASSERT(coll2);

   mongoc_collection_destroy(coll2);
   mongoc_collection_destroy(coll);
   mongoc_database_destroy(db);
   mongoc_client_destroy(client);
}

/* A "." in a collection name is unambiguous within a namespace, and stays legal. */
static void
test_ns_validation_dotted_collection_ok(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   mongoc_collection_t *const coll = mongoc_client_get_collection(client, "db", "coll.with.dots");
   bson_error_t error;

   ASSERT_CMPSTR(mongoc_collection_get_name(coll), "coll.with.dots");

   /* No client-side error: only an invalid database name is rejected. */
   bson_t *const filter = bson_new();
   mongoc_cursor_t *const cursor = mongoc_collection_find_with_opts(coll, filter, NULL, NULL);
   ASSERT(!mongoc_cursor_error(cursor, &error));

   mongoc_cursor_destroy(cursor);
   bson_destroy(filter);
   mongoc_collection_destroy(coll);
   mongoc_client_destroy(client);
}

/* Reads go through a cursor. `_mongoc_set_cursor_ns` splits the namespace at the first ".", so the check must happen
 * where the database and collection names are still separate. The error surfaces via `mongoc_cursor_error` without any
 * server round trip. */
static void
test_ns_validation_cursors(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   mongoc_database_t *const db = mongoc_client_get_database(client, BAD_DB);
   mongoc_collection_t *const coll = mongoc_client_get_collection(client, BAD_DB, "coll");
   bson_t *const empty = bson_new();
   bson_error_t error;
   mongoc_cursor_t *cursor;

   cursor = mongoc_collection_find_with_opts(coll, empty, NULL, NULL);
   ASSERT(mongoc_cursor_error(cursor, &error));
   EXPECT_BAD_DB(error);
   mongoc_cursor_destroy(cursor);

   cursor = mongoc_collection_aggregate(coll, MONGOC_QUERY_NONE, empty, NULL, NULL);
   ASSERT(mongoc_cursor_error(cursor, &error));
   EXPECT_BAD_DB(error);
   mongoc_cursor_destroy(cursor);

   cursor = mongoc_collection_find_indexes_with_opts(coll, NULL);
   ASSERT(mongoc_cursor_error(cursor, &error));
   EXPECT_BAD_DB(error);
   mongoc_cursor_destroy(cursor);

   cursor = mongoc_database_aggregate(db, empty, NULL, NULL);
   ASSERT(mongoc_cursor_error(cursor, &error));
   EXPECT_BAD_DB(error);
   mongoc_cursor_destroy(cursor);

   cursor = mongoc_database_find_collections_with_opts(db, NULL);
   ASSERT(mongoc_cursor_error(cursor, &error));
   EXPECT_BAD_DB(error);
   mongoc_cursor_destroy(cursor);

   bson_destroy(empty);
   mongoc_collection_destroy(coll);
   mongoc_database_destroy(db);
   mongoc_client_destroy(client);
}

/* Commands and writes pass the database name to `mongoc_cmd_parts_assemble` whole, which is the common backstop. These
 * require server selection to succeed first, so a live server is needed. */
static void
test_ns_validation_commands(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   mongoc_database_t *const db = mongoc_client_get_database(client, BAD_DB);
   mongoc_collection_t *const coll = mongoc_client_get_collection(client, BAD_DB, "coll");
   bson_error_t error;

   // A raw command.
   bson_t *const cmd = BCON_NEW("ping", BCON_INT32(1));
   ASSERT(!mongoc_client_command_simple(client, BAD_DB, cmd, NULL, NULL, &error));
   EXPECT_BAD_DB(error);
   ASSERT(!mongoc_database_command_simple(db, cmd, NULL, NULL, &error));
   EXPECT_BAD_DB(error);
   bson_destroy(cmd);

   // A write.
   bson_t *const doc = BCON_NEW("_id", BCON_INT32(1));
   ASSERT(!mongoc_collection_insert_one(coll, doc, NULL, NULL, &error));
   EXPECT_BAD_DB(error);
   bson_destroy(doc);

   // A command-based collection helper.
   ASSERT(!mongoc_collection_drop(coll, &error));
   EXPECT_BAD_DB(error);

   // A change stream: `_make_cursor` runs "aggregate" through the command path.
   bson_t *const empty = bson_new();
   mongoc_change_stream_t *const stream = mongoc_collection_watch(coll, empty, NULL);
   ASSERT(mongoc_change_stream_error_document(stream, &error, NULL));
   EXPECT_BAD_DB(error);
   mongoc_change_stream_destroy(stream);
   bson_destroy(empty);

   mongoc_collection_destroy(coll);
   mongoc_database_destroy(db);
   mongoc_client_destroy(client);
}

static void
test_ns_validation_rename(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   mongoc_collection_t *const coll = mongoc_client_get_collection(client, "db", "coll");
   bson_error_t error;

   ASSERT(!mongoc_collection_rename(coll, BAD_DB, "coll", false /* drop_target_before_rename */, &error));
   EXPECT_BAD_DB(error);

   mongoc_collection_destroy(coll);
   mongoc_client_destroy(client);
}

static void
test_ns_validation_get_gridfs(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   bson_error_t error;

   ASSERT(!mongoc_client_get_gridfs(client, BAD_DB, "fs", &error));
   EXPECT_BAD_DB(error);

   mongoc_client_destroy(client);
}

static void
test_ns_validation_gridfs_bucket(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   mongoc_database_t *const db = mongoc_client_get_database(client, "db");
   bson_error_t error;

   /* `bucketName` is read from a BSON string, so unlike every other name argument it can carry an embedded NUL. Without
    * validation, "fs\0bad" is truncated to the "fs.files" and "fs.chunks" collections. */
   bson_t *const opts = bson_new();
   ASSERT(bson_append_utf8(opts, "bucketName", -1, "fs\0bad", 6));

   ASSERT(!mongoc_gridfs_bucket_new(db, opts, NULL /* read_prefs */, &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_NAMESPACE, MONGOC_ERROR_NAMESPACE_INVALID, "invalid: contains a NUL byte");

   // A "." in `bucketName` is permitted.
   bson_t *const dotted_opts = BCON_NEW("bucketName", "fs.dotted");
   mongoc_gridfs_bucket_t *const bucket = mongoc_gridfs_bucket_new(db, dotted_opts, NULL /* read_prefs */, &error);
   ASSERT_OR_PRINT(bucket, error);

   mongoc_gridfs_bucket_destroy(bucket);
   bson_destroy(dotted_opts);
   bson_destroy(opts);
   mongoc_database_destroy(db);
   mongoc_client_destroy(client);
}

static void
test_ns_validation_uri_set_database(void)
{
   mongoc_uri_t *const uri = mongoc_uri_new("mongodb://localhost:27017");
   ASSERT(uri);

   /* `mongoc_uri_set_database` already returns false for an unusable name (e.g. invalid UTF-8), so rejecting here is
    * in-contract. */
   capture_logs(true);
   ASSERT(!mongoc_uri_set_database(uri, BAD_DB));
   ASSERT_CAPTURED_LOG("mongoc_uri_set_database", MONGOC_LOG_LEVEL_ERROR, "invalid: contains \".\"");
   clear_captured_logs();

   ASSERT(mongoc_uri_set_database(uri, "db"));
   ASSERT_NO_CAPTURED_LOGS("mongoc_uri_set_database");
   capture_logs(false);

   ASSERT_CMPSTR(mongoc_uri_get_database(uri), "db");

   mongoc_uri_destroy(uri);
}

static void
test_ns_validation_uri_path(void)
{
   bson_error_t error;

   // A "." in the URI path is rejected when parsing. This predates DRIVERS-3600.
   ASSERT(!mongoc_uri_new_with_error("mongodb://localhost:27017/db.bad", &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "disallowed characters");

   // ... including when %-encoded.
   ASSERT(!mongoc_uri_new_with_error("mongodb://localhost:27017/db%2Ebad", &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "disallowed characters");

   // A %-encoded NUL byte is rejected.
   ASSERT(!mongoc_uri_new_with_error("mongodb://localhost:27017/db%00bad", &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "null characters");
}

static void
test_ns_validation_auth_source(void)
{
   bson_error_t error;

   // `authSource` is a database name: it is joined with the username to form "saslSupportedMechs".
   ASSERT(!mongoc_uri_new_with_error("mongodb://user:pwd@localhost:27017/?authSource=db.bad", &error));
   ASSERT_ERROR_CONTAINS(error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "invalid: contains \".\"");

   mongoc_uri_t *const uri = mongoc_uri_new("mongodb://user:pwd@localhost:27017");
   ASSERT(uri);

   capture_logs(true);
   ASSERT(!mongoc_uri_set_auth_source(uri, BAD_DB));
   ASSERT_CAPTURED_LOG("mongoc_uri_set_auth_source", MONGOC_LOG_LEVEL_ERROR, "invalid: contains \".\"");
   clear_captured_logs();

   ASSERT(mongoc_uri_set_auth_source(uri, "$external"));
   ASSERT_NO_CAPTURED_LOGS("mongoc_uri_set_auth_source");
   capture_logs(false);

   mongoc_uri_destroy(uri);
}

#ifdef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION
static void
test_ns_validation_keyvault_namespace(void)
{
   mongoc_client_t *const client = test_framework_new_default_client();
   bson_error_t error;

   bson_t *const kms_providers =
      BCON_NEW("local", "{", "key", BCON_BIN(BSON_SUBTYPE_BINARY, (uint8_t[96]){0}, 96), "}");

   // A "." in the key vault database name would target a different key vault collection than requested.
   {
      mongoc_auto_encryption_opts_t *const opts = mongoc_auto_encryption_opts_new();
      mongoc_auto_encryption_opts_set_keyvault_namespace(opts, "keyvault.bad", "datakeys");
      mongoc_auto_encryption_opts_set_kms_providers(opts, kms_providers);

      ASSERT(!mongoc_client_enable_auto_encryption(client, opts, &error));
      EXPECT_BAD_DB(error);

      mongoc_auto_encryption_opts_destroy(opts);
   }

   {
      mongoc_client_encryption_opts_t *const opts = mongoc_client_encryption_opts_new();
      mongoc_client_encryption_opts_set_keyvault_namespace(opts, "keyvault.bad", "datakeys");
      mongoc_client_encryption_opts_set_keyvault_client(opts, client);
      mongoc_client_encryption_opts_set_kms_providers(opts, kms_providers);

      ASSERT(!mongoc_client_encryption_new(opts, &error));
      EXPECT_BAD_DB(error);

      mongoc_client_encryption_opts_destroy(opts);
   }

   bson_destroy(kms_providers);
   mongoc_client_destroy(client);
}
#endif /* MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION */

void
test_ns_validation_install(TestSuite *suite)
{
   TestSuite_AddLive(suite, "/ns_validation/handles_still_created", test_ns_validation_handles_still_created);
   TestSuite_AddLive(suite, "/ns_validation/dotted_collection_ok", test_ns_validation_dotted_collection_ok);
   TestSuite_AddLive(suite, "/ns_validation/cursors", test_ns_validation_cursors);
   TestSuite_AddLive(suite, "/ns_validation/commands", test_ns_validation_commands);
   TestSuite_AddLive(suite, "/ns_validation/rename", test_ns_validation_rename);
   TestSuite_AddLive(suite, "/ns_validation/get_gridfs", test_ns_validation_get_gridfs);
   TestSuite_AddLive(suite, "/ns_validation/gridfs_bucket", test_ns_validation_gridfs_bucket);
   TestSuite_Add(suite, "/ns_validation/uri_set_database", test_ns_validation_uri_set_database);
   TestSuite_Add(suite, "/ns_validation/uri_path", test_ns_validation_uri_path);
   TestSuite_Add(suite, "/ns_validation/auth_source", test_ns_validation_auth_source);
#ifdef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION
   TestSuite_AddLive(suite, "/ns_validation/keyvault_namespace", test_ns_validation_keyvault_namespace);
#endif
}
