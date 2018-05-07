/*
 * Copyright 2018-present MongoDB, Inc.
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


#include <mongoc-cursor-private.h>
#include "mongoc-config.h"
#include "mongoc-collection-private.h"
#include "mongoc-host-list-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-util-private.h"
#include "mongoc-util-private.h"

#include "json-test.h"
#include "json-test-operations.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"


void
json_test_ctx_init (json_test_ctx_t *ctx,
                    const bson_t *test,
                    mongoc_client_t *client,
                    const json_test_config_t *config)
{
   char *session_name;
   char *session_opts_path;
   int i;
   bson_error_t error;

   ctx->config = config;
   ctx->n_events = 0;
   bson_init (&ctx->events);
   ctx->test_framework_uri = test_framework_get_uri ();
   ctx->cursor_id = 0;
   ctx->acknowledged = true;
   ctx->verbose = test_framework_getenv_bool ("MONGOC_TEST_MONITORING_VERBOSE");
   bson_init (&ctx->lsids[0]);
   bson_init (&ctx->lsids[1]);
   ctx->sessions[0] = NULL;
   ctx->sessions[1] = NULL;
   ctx->has_sessions = test_framework_session_timeout_minutes () > -1 &&
                       test_framework_skip_if_no_crypto ();

   /* transactions tests require two sessions named session1 and session2,
    * retryable writes use one explicit session or none */
   if (ctx->has_sessions) {
      for (i = 0; i < 2; i++) {
         session_name = bson_strdup_printf ("session%d", i);
         session_opts_path = bson_strdup_printf ("sessionOptions.session%d", i);
         if (bson_has_field (test, session_opts_path)) {
            ctx->sessions[i] =
               bson_lookup_session (test, session_opts_path, client);
         } else {
            ctx->sessions[i] =
               mongoc_client_start_session (client, NULL, &error);
         }

         ASSERT_OR_PRINT (ctx->sessions[i], error);
         bson_concat (&ctx->lsids[i],
                      mongoc_client_session_get_lsid (ctx->sessions[i]));

         bson_free (session_name);
         bson_free (session_opts_path);
      }
   }
}


void
json_test_ctx_end_sessions (json_test_ctx_t *ctx)
{
   int i;

   if (ctx->has_sessions) {
      for (i = 0; i < 2; i++) {
         if (ctx->sessions[i]) {
            mongoc_client_session_destroy (ctx->sessions[i]);
            ctx->sessions[i] = NULL;
         }
      }
   }
}


void
json_test_ctx_cleanup (json_test_ctx_t *ctx)
{
   json_test_ctx_end_sessions (ctx);
   bson_destroy (&ctx->lsids[0]);
   bson_destroy (&ctx->lsids[1]);
   bson_destroy (&ctx->events);
   mongoc_uri_destroy (ctx->test_framework_uri);
}


static void
append_session (mongoc_client_session_t *session, bson_t *opts)
{
   if (session) {
      bool r;
      bson_error_t error;

      r = mongoc_client_session_append (session, opts, &error);
      ASSERT_OR_PRINT (r, error);
   }
}


static bool
get_successful_result (const bson_t *test,
                       const bson_t *operation,
                       bson_t *result)
{
   /* retryable writes tests specify result at the end of the whole test:
    *   operation:
    *     name: insertOne
    *     arguments: ...
    *   outcome:
    *     result:
    *       insertedId: 3
    *
    * transactions tests specify the result of each operation:
    *    operations:
    *      - name: insertOne
    *        arguments: ...
    *        result:
    *          insertedId: 3
    *
    * command monitoring tests have no results
    */
   if (bson_has_field (test, "outcome.result")) {
      bson_lookup_doc (test, "outcome.result", result);
   } else if (bson_has_field (operation, "result")) {
      bson_lookup_doc (operation, "result", result);
   } else {
      return false;
   }

   return true;
}


static void
check_success_expected (const bson_t *operation,
                        bool succeeded,
                        bool expected,
                        const bson_error_t *error)
{
   char *json = bson_as_json (operation, NULL);

   if (!succeeded && expected) {
      test_error (
         "Expected success, got error \"%s\":\n%s", error->message, json);
   }
   if (succeeded && !expected) {
      test_error ("Expected error, got success:\n%s", json);
   }

   bson_free (json);
}


static void
check_result (const bson_t *test,
              const bson_t *operation,
              bool succeeded,
              const bson_t *reply,
              const bson_error_t *error)
{
   /* retryable writes tests specify error: false at the end of the whole test:
    *   operation:
    *     name: insertOne
    *   outcome:
    *     error: true
    */
   if (bson_has_field (test, "outcome.result.error")) {
      check_success_expected (
         operation,
         succeeded,
         _mongoc_lookup_bool (test, "outcome.result.error", false),
         error);
   }
   /* transactions tests specify server error code name per-operation:
    *    operations:
    *      - name: insertOne
    *        arguments: ...
    *        result:
    *          errorCodeName: WriteConflict
    */
   else if (bson_has_field (operation, "result.errorCodeName")) {
      check_success_expected (operation, succeeded, false, error);
      /* tests with errorCodeName should exercise only functions with replies */
      BSON_ASSERT (reply);
   }
   /* transactions tests specify client error message per-operation:
    *    operations:
    *      - name: insertOne
    *        arguments: ...
    *        result:
    *          errorContains: "message substring"
    */
   else if (bson_has_field (operation, "result.errorContains")) {
      const char *msg;

      check_success_expected (operation, succeeded, false, error);
      msg = bson_lookup_utf8 (operation, "result.errorContains");
      ASSERT_CONTAINS (error->message, msg);
   } else if (bson_has_field (operation, "result")) {
      /* operation expected to succeed */
      check_success_expected (operation, succeeded, true, error);
   }

   /* if there's no "result", e.g. in the command monitoring tests, we don't
    * know if the command is expected to succeed or fail */
}


static void
add_request_to_bulk (mongoc_bulk_operation_t *bulk, const bson_t *request)
{
   const char *name;
   bson_t args;
   bool r;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;

   name = bson_lookup_utf8 (request, "name");
   bson_lookup_doc (request, "arguments", &args);

   if (!strcmp (name, "deleteMany")) {
      bson_t filter;

      bson_lookup_doc (&args, "filter", &filter);

      r = mongoc_bulk_operation_remove_many_with_opts (
         bulk, &filter, &opts, &error);
   } else if (!strcmp (name, "deleteOne")) {
      bson_t filter;

      bson_lookup_doc (&args, "filter", &filter);

      r = mongoc_bulk_operation_remove_one_with_opts (
         bulk, &filter, &opts, &error);
   } else if (!strcmp (name, "insertOne")) {
      bson_t document;

      bson_lookup_doc (&args, "document", &document);

      r = mongoc_bulk_operation_insert_with_opts (
         bulk, &document, &opts, &error);
   } else if (!strcmp (name, "replaceOne")) {
      bson_t filter;
      bson_t replacement;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "replacement", &replacement);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_replace_one_with_opts (
         bulk, &filter, &replacement, &opts, &error);
   } else if (!strcmp (name, "updateMany")) {
      bson_t filter;
      bson_t update;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "update", &update);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_update_many_with_opts (
         bulk, &filter, &update, &opts, &error);
   } else if (!strcmp (name, "updateOne")) {
      bson_t filter;
      bson_t update;

      bson_lookup_doc (&args, "filter", &filter);
      bson_lookup_doc (&args, "update", &update);

      if (bson_has_field (&args, "upsert")) {
         BSON_APPEND_BOOL (
            &opts, "upsert", _mongoc_lookup_bool (&args, "upsert", false));
      }

      r = mongoc_bulk_operation_update_one_with_opts (
         bulk, &filter, &update, &opts, &error);
   } else {
      test_error ("unrecognized request name %s", name);
      abort ();
   }

   ASSERT_OR_PRINT (r, error);

   bson_destroy (&opts);
}


static bson_t *
convert_spec_result_to_bulk_write_result (const bson_t *spec_result)
{
   bson_t *result;
   bson_iter_t iter;

   result = tmp_bson ("{}");

   ASSERT (bson_iter_init (&iter, spec_result));

   while (bson_iter_next (&iter)) {
      /* libmongoc does not report inserted IDs, so ignore those fields */
      if (BSON_ITER_IS_KEY (&iter, "insertedCount")) {
         BSON_APPEND_VALUE (result, "nInserted", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "deletedCount")) {
         BSON_APPEND_VALUE (result, "nRemoved", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "matchedCount")) {
         BSON_APPEND_VALUE (result, "nMatched", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "modifiedCount")) {
         BSON_APPEND_VALUE (result, "nModified", bson_iter_value (&iter));
      }
      if (BSON_ITER_IS_KEY (&iter, "upsertedCount")) {
         BSON_APPEND_VALUE (result, "nUpserted", bson_iter_value (&iter));
      }
      /* convert a single-statement upsertedId result field to a bulk write
       * upsertedIds result field */
      if (BSON_ITER_IS_KEY (&iter, "upsertedId")) {
         bson_t upserted;
         bson_t upsert;

         BSON_APPEND_ARRAY_BEGIN (result, "upserted", &upserted);
         BSON_APPEND_DOCUMENT_BEGIN (&upserted, "0", &upsert);
         BSON_APPEND_INT32 (&upsert, "index", 0);
         BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&iter));
         bson_append_document_end (&upserted, &upsert);
         bson_append_array_end (result, &upserted);
      }
      if (BSON_ITER_IS_KEY (&iter, "upsertedIds")) {
         bson_t upserted;
         bson_iter_t inner;
         uint32_t i = 0;

         ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));

         /* include the "upserted" field if upsertedIds isn't empty */
         ASSERT (bson_iter_recurse (&iter, &inner));
         while (bson_iter_next (&inner)) {
            i++;
         }

         if (i) {
            i = 0;
            ASSERT (bson_iter_recurse (&iter, &inner));
            BSON_APPEND_ARRAY_BEGIN (result, "upserted", &upserted);

            while (bson_iter_next (&inner)) {
               bson_t upsert;
               const char *keyptr = NULL;
               char key[12];

               bson_uint32_to_string (i++, &keyptr, key, sizeof key);

               BSON_APPEND_DOCUMENT_BEGIN (&upserted, keyptr, &upsert);
               BSON_APPEND_INT32 (
                  &upsert, "index", atoi (bson_iter_key (&inner)));
               BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&inner));
               bson_append_document_end (&upserted, &upsert);
            }

            bson_append_array_end (result, &upserted);
         }
      }
   }

   return result;
}


static void
execute_bulk_operation (mongoc_bulk_operation_t *bulk,
                        const bson_t *test,
                        const bson_t *operation)
{
   uint32_t server_id;
   bson_error_t error;
   bson_t reply;
   bson_t spec_result;
   bson_t *expected_result;

   server_id = mongoc_bulk_operation_execute (bulk, &reply, &error);
   check_result (test, operation, server_id != 0, &reply, &error);
   if (get_successful_result (test, operation, &spec_result)) {
      expected_result = convert_spec_result_to_bulk_write_result (&spec_result);
      ASSERT (match_bson (&reply, expected_result, false));
   }

   bson_destroy (&reply);
}


static bson_t *
create_bulk_write_opts (const bson_t *operation,
                        mongoc_client_session_t *session,
                        mongoc_write_concern_t *wc)
{
   bson_t *opts;
   bson_t tmp;

   opts = tmp_bson ("{}");

   if (bson_has_field (operation, "arguments.options")) {
      bson_lookup_doc (operation, "arguments.options", &tmp);
      bson_concat (opts, &tmp);
   }

   append_session (session, opts);

   if (wc) {
      BSON_ASSERT (mongoc_write_concern_append (wc, opts));
   }

   return opts;
}


static void
bulk_write (mongoc_collection_t *collection,
            const bson_t *test,
            const bson_t *operation,
            mongoc_client_session_t *session,
            mongoc_write_concern_t *wc)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t requests;
   bson_iter_t iter;

   opts = create_bulk_write_opts (operation, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (operation, "arguments.requests", &requests);
   ASSERT (bson_iter_init (&iter, &requests));

   while (bson_iter_next (&iter)) {
      bson_t request;

      bson_iter_bson (&iter, &request);
      add_request_to_bulk (bulk, &request);
   }

   bson_destroy (&requests);
   execute_bulk_operation (bulk, test, operation);
   mongoc_bulk_operation_destroy (bulk);
}


static void
single_write (mongoc_collection_t *collection,
              const bson_t *test,
              const bson_t *operation,
              mongoc_client_session_t *session,
              mongoc_write_concern_t *wc)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;

   /* for ease, use bulk for all writes, not mongoc_collection_insert_one etc */
   opts = create_bulk_write_opts (test, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   add_request_to_bulk (bulk, operation);
   execute_bulk_operation (bulk, test, operation);
   mongoc_bulk_operation_destroy (bulk);
}


static mongoc_find_and_modify_opts_t *
create_find_and_modify_opts (const char *name,
                             const bson_t *args,
                             mongoc_client_session_t *session,
                             mongoc_write_concern_t *wc)
{
   mongoc_find_and_modify_opts_t *opts;
   mongoc_find_and_modify_flags_t flags = MONGOC_FIND_AND_MODIFY_NONE;
   bson_t extra = BSON_INITIALIZER;

   opts = mongoc_find_and_modify_opts_new ();

   if (!strcmp (name, "findOneAndDelete")) {
      flags |= MONGOC_FIND_AND_MODIFY_REMOVE;
   }

   if (!strcmp (name, "findOneAndReplace")) {
      bson_t replacement;
      bson_lookup_doc (args, "replacement", &replacement);
      mongoc_find_and_modify_opts_set_update (opts, &replacement);
   }

   if (!strcmp (name, "findOneAndUpdate")) {
      bson_t update;
      bson_lookup_doc (args, "update", &update);
      mongoc_find_and_modify_opts_set_update (opts, &update);
   }

   if (bson_has_field (args, "sort")) {
      bson_t sort;
      bson_lookup_doc (args, "sort", &sort);
      mongoc_find_and_modify_opts_set_sort (opts, &sort);
   }

   if (_mongoc_lookup_bool (args, "upsert", false)) {
      flags |= MONGOC_FIND_AND_MODIFY_UPSERT;
   }

   if (bson_has_field (args, "returnDocument") &&
       !strcmp ("After", bson_lookup_utf8 (args, "returnDocument"))) {
      flags |= MONGOC_FIND_AND_MODIFY_RETURN_NEW;
   }

   mongoc_find_and_modify_opts_set_flags (opts, flags);
   append_session (session, &extra);

   if (!mongoc_write_concern_is_default (wc)) {
      BSON_ASSERT (mongoc_write_concern_append (wc, &extra));
   }

   ASSERT (mongoc_find_and_modify_opts_append (opts, &extra));
   bson_destroy (&extra);

   return opts;
}


static void
find_and_modify (mongoc_collection_t *collection,
                 const bson_t *test,
                 const bson_t *operation,
                 mongoc_client_session_t *session,
                 mongoc_write_concern_t *wc)
{
   const char *name;
   bson_t args;
   bson_t filter;
   mongoc_find_and_modify_opts_t *opts;
   bson_t reply;
   bson_error_t error;
   bool r;

   name = bson_lookup_utf8 (operation, "name");
   bson_lookup_doc (operation, "arguments", &args);
   bson_lookup_doc (operation, "arguments.filter", &filter);

   opts = create_find_and_modify_opts (name, &args, session, wc);
   r = mongoc_collection_find_and_modify_with_opts (
      collection, &filter, opts, &reply, &error);

   check_result (test, operation, r, &reply, &error);

   mongoc_find_and_modify_opts_destroy (opts);
   bson_destroy (&reply);
   bson_destroy (&args);
   bson_destroy (&filter);
}


static void
insert_many (mongoc_collection_t *collection,
             const bson_t *test,
             const bson_t *operation,
             mongoc_client_session_t *session,
             mongoc_write_concern_t *wc)
{
   bson_t documents;
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_iter_t iter;

   opts = create_bulk_write_opts (operation, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (operation, "arguments.documents", &documents);
   ASSERT (bson_iter_init (&iter, &documents));
   while (bson_iter_next (&iter)) {
      bson_t document;
      bool r;
      bson_error_t error;

      bson_iter_bson (&iter, &document);
      r =
         mongoc_bulk_operation_insert_with_opts (bulk, &document, NULL, &error);
      ASSERT_OR_PRINT (r, error);
   }

   execute_bulk_operation (bulk, test, operation);
   bson_destroy (&documents);
   mongoc_bulk_operation_destroy (bulk);
}


static void
count (mongoc_collection_t *collection,
       const bson_t *test,
       const bson_t *operation,
       mongoc_client_session_t *session,
       const mongoc_read_prefs_t *read_prefs)
{
   bson_t filter;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;
   int64_t r;

   bson_lookup_doc (operation, "arguments.filter", &filter);
   append_session (session, &opts);
   r = mongoc_collection_count_with_opts (
      collection, MONGOC_QUERY_NONE, &filter, 0, 0, &opts, read_prefs, &error);

   check_result (test, operation, r > -1, NULL, &error);

   bson_destroy (&opts);
}


static void
distinct (mongoc_collection_t *collection,
          const bson_t *test,
          const bson_t *operation,
          mongoc_client_session_t *session,
          const mongoc_read_prefs_t *read_prefs)
{
   bson_t opts = BSON_INITIALIZER;
   const char *field_name;
   bson_t reply;
   bson_error_t error;
   bool r;

   append_session (session, &opts);
   field_name = bson_lookup_utf8 (operation, "arguments.fieldName");
   r = mongoc_collection_read_command_with_opts (
      collection,
      tmp_bson (
         "{'distinct': '%s', 'key': '%s'}", collection->collection, field_name),
      read_prefs,
      &opts,
      &reply,
      &error);

   check_result (test, operation, r, &reply, &error);

   bson_destroy (&reply);
   bson_destroy (&opts);
}


static void
check_cursor (mongoc_cursor_t *cursor,
              const bson_t *test,
              const bson_t *operation)
{
   const bson_t *doc;
   bson_error_t error;

   while (mongoc_cursor_next (cursor, &doc)) {
   }

   check_result (
      test, operation, !mongoc_cursor_error (cursor, &error), NULL, &error);
}


static void
find (mongoc_collection_t *collection,
      const bson_t *test,
      const bson_t *operation,
      mongoc_client_session_t *session,
      const mongoc_read_prefs_t *read_prefs)
{
   bson_t arguments;
   bson_t tmp;
   bson_t filter;
   bson_t opts = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;
   bson_error_t error;

   bson_lookup_doc (operation, "arguments", &arguments);
   if (bson_has_field (&arguments, "filter")) {
      bson_lookup_doc (&arguments, "filter", &tmp);
      bson_copy_to (&tmp, &filter);
   } else {
      bson_init (&filter);
   }

   /* Command Monitoring Spec tests use OP_QUERY-style modifiers for "find":
    *   arguments:
    *    filter: { _id: { $gt: 1 } }
    *    sort: { _id: 1 }
    *    skip: {"$numberLong": "2"}
    *    modifiers:
    *      $comment: "test"
    *      $showDiskLoc: false
    *
    * Abuse _mongoc_cursor_translate_dollar_query_opts to upgrade "modifiers".
    */
   if (bson_has_field (&arguments, "modifiers")) {
      bson_t modifiers;
      bson_t *query = tmp_bson ("{'$query': {}}");
      bson_t unwrapped;
      bool r;

      bson_lookup_doc (&arguments, "modifiers", &modifiers);
      bson_concat (query, &modifiers);
      r = _mongoc_cursor_translate_dollar_query_opts (
         query, &opts, &unwrapped, &error);
      ASSERT_OR_PRINT (r, error);
      bson_destroy (&unwrapped);
   }

   bson_copy_to_excluding_noinit (&arguments,
                                  &opts,
                                  "filter",
                                  "modifiers",
                                  "readPreference",
                                  "session",
                                  NULL);

   append_session (session, &opts);

   cursor =
      mongoc_collection_find_with_opts (collection, &filter, &opts, read_prefs);

   check_cursor (cursor, test, operation);
   mongoc_cursor_destroy (cursor);
   bson_destroy (&filter);
   bson_destroy (&opts);
}


static void
aggregate (mongoc_collection_t *collection,
           const bson_t *test,
           const bson_t *operation,
           mongoc_client_session_t *session,
           const mongoc_read_prefs_t *read_prefs)
{
   bson_t arguments;
   bson_t pipeline;
   bson_t opts = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;

   bson_lookup_doc (operation, "arguments", &arguments);
   bson_lookup_doc (&arguments, "pipeline", &pipeline);
   append_session (session, &opts);
   bson_copy_to_excluding_noinit (
      &arguments, &opts, "pipeline", "session", "readPreference", NULL);

   cursor = mongoc_collection_aggregate (
      collection, MONGOC_QUERY_NONE, &pipeline, &opts, read_prefs);

   check_cursor (cursor, test, operation);
   mongoc_cursor_destroy (cursor);
   bson_destroy (&opts);
}


static void
start_transaction (const bson_t *test,
                   const bson_t *operation,
                   mongoc_client_session_t *session)
{
   mongoc_transaction_opt_t *opts = NULL;
   bson_error_t error;
   bool r;

   if (bson_has_field (operation, "arguments.options")) {
      opts = bson_lookup_txn_opts (operation, "arguments.options");
   }

   r = mongoc_client_session_start_transaction (session, opts, &error);
   check_result (test, operation, r, NULL, &error);

   if (opts) {
      mongoc_transaction_opts_destroy (opts);
   }
}


static void
commit_transaction (const bson_t *test,
                    const bson_t *operation,
                    mongoc_client_session_t *session)
{
   bson_t reply;
   bson_error_t error;
   bool r;

   r = mongoc_client_session_commit_transaction (session, &reply, &error);
   check_result (test, operation, r, &reply, &error);

   bson_destroy (&reply);
}


static void
abort_transaction (const bson_t *test,
                   const bson_t *operation,
                   mongoc_client_session_t *session)
{
   bson_error_t error;
   bool r;

   r = mongoc_client_session_abort_transaction (session, &error);
   check_result (test, operation, r, NULL, &error);
}


void
json_test_operation (const bson_t *test,
                     const bson_t *operation,
                     mongoc_collection_t *collection,
                     mongoc_client_session_t *session)
{
   const char *op_name;
   mongoc_read_prefs_t *read_prefs = NULL;
   mongoc_write_concern_t *wc;

   op_name = bson_lookup_utf8 (operation, "name");
   if (bson_has_field (operation, "read_preference")) {
      /* command monitoring tests */
      read_prefs = bson_lookup_read_prefs (operation, "read_preference");
   } else if (bson_has_field (operation, "arguments.readPreference")) {
      /* transactions tests */
      read_prefs =
         bson_lookup_read_prefs (operation, "arguments.readPreference");
   }

   if (bson_has_field (operation, "arguments.writeConcern")) {
      wc = bson_lookup_write_concern (operation, "arguments.writeConcern");
   } else {
      wc = mongoc_write_concern_new ();
   }

   if (!strcmp (op_name, "bulkWrite")) {
      bulk_write (collection, test, operation, session, wc);
   } else if (!strcmp (op_name, "deleteOne") ||
              !strcmp (op_name, "deleteMany") ||
              !strcmp (op_name, "insertOne") ||
              !strcmp (op_name, "replaceOne") ||
              !strcmp (op_name, "updateOne") ||
              !strcmp (op_name, "updateMany")) {
      single_write (collection, test, operation, session, wc);
   } else if (!strcmp (op_name, "findOneAndDelete") ||
              !strcmp (op_name, "findOneAndReplace") ||
              !strcmp (op_name, "findOneAndUpdate")) {
      find_and_modify (collection, test, operation, session, wc);
   } else if (!strcmp (op_name, "insertMany")) {
      insert_many (collection, test, operation, session, wc);
   } else if (!strcmp (op_name, "count")) {
      count (collection, test, operation, session, read_prefs);
   } else if (!strcmp (op_name, "distinct")) {
      distinct (collection, test, operation, session, read_prefs);
   } else if (!strcmp (op_name, "find")) {
      find (collection, test, operation, session, read_prefs);
   } else if (!strcmp (op_name, "aggregate")) {
      aggregate (collection, test, operation, session, read_prefs);
   } else if (!strcmp (op_name, "startTransaction")) {
      start_transaction (test, operation, session);
   } else if (!strcmp (op_name, "commitTransaction")) {
      commit_transaction (test, operation, session);
   } else if (!strcmp (op_name, "abortTransaction")) {
      abort_transaction (test, operation, session);
   } else {
      test_error ("unrecognized operation name %s", op_name);
   }

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_write_concern_destroy (wc);
}


static void
one_operation (const json_test_config_t *config,
               json_test_ctx_t *ctx,
               const bson_t *test,
               const bson_t *operation,
               mongoc_collection_t *collection)
{
   const char *op_name;
   mongoc_write_concern_t *wc;

   op_name = bson_lookup_utf8 (operation, "name");
   if (ctx->verbose) {
      printf ("     %s\n", op_name);
      fflush (stdout);
   }

   if (bson_has_field (operation, "arguments.writeConcern")) {
      wc = bson_lookup_write_concern (operation, "arguments.writeConcern");
      ctx->acknowledged = mongoc_write_concern_is_acknowledged (wc);
      mongoc_write_concern_destroy (wc);
   } else {
      ctx->acknowledged = true;
   }

   if (config->run_operation_cb) {
      config->run_operation_cb (ctx, test, operation, collection);
   } else {
      test_error ("set json_test_config_t.run_operation_cb to a callback"
                  " that executes json_test_operation()");
   }
}


void
json_test_operations (const json_test_config_t *config,
                      json_test_ctx_t *ctx,
                      const bson_t *test,
                      mongoc_collection_t *collection)
{
   bson_t operations;
   bson_t operation;
   bson_iter_t iter;

   /* run each CRUD operation in the test, using the config's run-operation
    * callback, by default json_test_operation(). retryable writes tests have
    * one operation each, transactions tests have an array of them. */
   if (bson_has_field (test, "operation")) {
      bson_lookup_doc (test, "operation", &operation);
      one_operation (config, ctx, test, &operation, collection);
   } else {
      bson_lookup_doc (test, "operations", &operations);
      ASSERT_CMPUINT32 (bson_count_keys (&operations), >, (uint32_t) 0);
      BSON_ASSERT (bson_iter_init (&iter, &operations));
      while (bson_iter_next (&iter)) {
         bson_iter_bson (&iter, &operation);
         one_operation (config, ctx, test, &operation, collection);
      }
   }
}
