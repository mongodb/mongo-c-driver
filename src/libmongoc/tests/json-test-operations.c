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


#include "bson.h"

#include "mongoc-collection-private.h"
#include "mongoc-config.h"
#include "mongoc-cursor-private.h"
#include "mongoc-host-list-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-util-private.h"
#include "mongoc-util-private.h"

#include "json-test-operations.h"
#include "json-test.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"


mongoc_client_session_t *
session_from_name (json_test_ctx_t *ctx, const char *session_name)
{
   if (!session_name) {
      return NULL;
   } else if (!strcmp (session_name, "session0")) {
      return ctx->sessions[0];
   } else if (!strcmp (session_name, "session1")) {
      return ctx->sessions[1];
   } else {
      MONGOC_ERROR ("Unrecognized session name: %s", session_name);
      abort ();
   }
}


void
json_test_ctx_init (json_test_ctx_t *ctx,
                    const bson_t *test,
                    mongoc_client_t *client,
                    mongoc_database_t *db,
                    mongoc_collection_t *collection,
                    const json_test_config_t *config)
{
   char *session_name;
   char *session_opts_path;
   int i;
   bson_error_t error;

   ctx->client = client;
   ctx->db = db;
   ctx->collection = collection;
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


static void
value_init_from_doc (bson_value_t *value, const bson_t *doc)
{
   value->value_type = BSON_TYPE_DOCUMENT;
   value->value.v_doc.data = bson_malloc ((size_t) doc->len);
   memcpy (value->value.v_doc.data, bson_get_data (doc), (size_t) doc->len);
   value->value.v_doc.data_len = doc->len;
}


static char *
value_to_str (const bson_value_t *value)
{
   bson_t doc;

   if (value->value_type == BSON_TYPE_DOCUMENT ||
       value->value_type == BSON_TYPE_ARRAY) {
      bson_init_from_value (&doc, value);
      return bson_as_json (&doc, NULL);
   } else {
      return bson_strdup_printf ("%" PRId64, bson_value_as_int64 (value));
   }
}


/* convert from spec result in JSON test to a libmongoc result */
static void
convert_spec_result (const bson_value_t *spec_result, bson_value_t *converted)
{
   bson_t doc;
   bson_t r;
   bson_iter_t iter;

   if (spec_result->value_type != BSON_TYPE_DOCUMENT &&
       spec_result->value_type != BSON_TYPE_ARRAY) {
      bson_value_copy (spec_result, converted);
      return;
   }

   bson_init (&r);
   bson_init_from_value (&doc, spec_result);
   ASSERT (bson_iter_init (&iter, &doc));

   while (bson_iter_next (&iter)) {
      /* libmongoc does not report inserted IDs, so ignore those fields */
      if (BSON_ITER_IS_KEY (&iter, "insertedCount")) {
         BSON_APPEND_VALUE (&r, "nInserted", bson_iter_value (&iter));
      } else if (BSON_ITER_IS_KEY (&iter, "deletedCount")) {
         BSON_APPEND_VALUE (&r, "nRemoved", bson_iter_value (&iter));
      } else if (BSON_ITER_IS_KEY (&iter, "matchedCount")) {
         BSON_APPEND_VALUE (&r, "nMatched", bson_iter_value (&iter));
      } else if (BSON_ITER_IS_KEY (&iter, "modifiedCount")) {
         BSON_APPEND_VALUE (&r, "nModified", bson_iter_value (&iter));
      } else if (BSON_ITER_IS_KEY (&iter, "upsertedCount")) {
         BSON_APPEND_VALUE (&r, "nUpserted", bson_iter_value (&iter));
      }
      /* some JSON tests have a single-write upsertedId field, some have a bulk
       * write upsertedIds array. we always return an array named "upserted". */
      else if (BSON_ITER_IS_KEY (&iter, "upsertedId")) {
         bson_t upserted;
         bson_t upsert;

         BSON_APPEND_ARRAY_BEGIN (&r, "upserted", &upserted);
         BSON_APPEND_DOCUMENT_BEGIN (&upserted, "0", &upsert);
         BSON_APPEND_INT32 (&upsert, "index", 0);
         BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&iter));
         bson_append_document_end (&upserted, &upsert);
         bson_append_array_end (&r, &upserted);
      } else if (BSON_ITER_IS_KEY (&iter, "upsertedIds")) {
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
            BSON_APPEND_ARRAY_BEGIN (&r, "upserted", &upserted);

            while (bson_iter_next (&inner)) {
               bson_t upsert;
               const char *keyptr = NULL;
               char key[12];
               int64_t index;

               bson_uint32_to_string (i++, &keyptr, key, sizeof key);
               index = bson_ascii_strtoll (bson_iter_key (&inner), NULL, 10);

               BSON_APPEND_DOCUMENT_BEGIN (&upserted, keyptr, &upsert);
               BSON_APPEND_INT32 (&upsert, "index", (int32_t) index);
               BSON_APPEND_VALUE (&upsert, "_id", bson_iter_value (&inner));
               bson_append_document_end (&upserted, &upsert);
            }

            bson_append_array_end (&r, &upserted);
         }
      } else if (BSON_ITER_IS_KEY (&iter, "insertedId")) {
         BSON_APPEND_INT64 (&r, "nInserted", (int64_t) 1);
      } else if (BSON_ITER_IS_KEY (&iter, "insertedIds")) {
         bson_t inserted_ids;
         bson_iter_bson (&iter, &inserted_ids);
         BSON_APPEND_INT64 (&r, "nInserted", bson_count_keys (&inserted_ids));
      } else {
         BSON_APPEND_VALUE (&r, bson_iter_key (&iter), bson_iter_value (&iter));
      }
   }

   /* copies r's contents */
   value_init_from_doc (converted, &r);
   /* preserve spec tests' distinction between array and document */
   converted->value_type = spec_result->value_type;
   bson_destroy (&r);
}


static bool
get_successful_result (const bson_t *test,
                       const bson_t *operation,
                       bson_value_t *value)
{
   bson_value_t pre_conversion;

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
      bson_lookup_value (test, "outcome.result", &pre_conversion);
   } else if (bson_has_field (operation, "result")) {
      bson_lookup_value (operation, "result", &pre_conversion);
   } else {
      return false;
   }

   convert_spec_result (&pre_conversion, value);
   bson_value_destroy (&pre_conversion);
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


static uint32_t
error_code_from_name (const char *name)
{
   if (!strcmp (name, "CannotSatisfyWriteConcern")) {
      return 100;
   } else if (!strcmp (name, "DuplicateKey")) {
      return 11000;
   } else if (!strcmp (name, "NoSuchTransaction")) {
      return 251;
   } else if (!strcmp (name, "WriteConflict")) {
      return 112;
   } else if (!strcmp (name, "Interrupted")) {
      return 11601;
   }

   test_error ("Add errorCodeName \"%s\" to error_code_from_name()", name);

   /* test_error() aborts, but add a return to suppress compiler warnings */
   return 0;
}


static void
check_error_code_name (const bson_t *operation, const bson_error_t *error)
{
   const char *code_name;

   if (!bson_has_field (operation, "result.errorCodeName")) {
      return;
   }

   code_name = bson_lookup_utf8 (operation, "result.errorCodeName");
   ASSERT_CMPUINT32 (error->code, ==, error_code_from_name (code_name));
}


static void
check_error_contains (const bson_t *operation, const bson_error_t *error)
{
   const char *msg;

   if (!bson_has_field (operation, "result.errorContains")) {
      return;
   }

   msg = bson_lookup_utf8 (operation, "result.errorContains");
   ASSERT_CONTAINS (error->message, msg);
}


static void
check_error_labels_contain (const bson_t *operation, const bson_value_t *result)
{
   bson_t reply;
   bson_iter_t operation_iter;
   bson_iter_t expected_labels;
   bson_iter_t expected_label;
   const char *expected_label_str;
   bson_t labels;

   if (!bson_has_field (operation, "result.errorLabelsContain")) {
      return;
   }

   bson_iter_init (&operation_iter, operation);
   BSON_ASSERT (bson_iter_find_descendant (
      &operation_iter, "result.errorLabelsContain", &expected_labels));
   BSON_ASSERT (bson_iter_recurse (&expected_labels, &expected_label));

   /* if the test has "errorLabelsContain" then result must be an error reply */
   ASSERT_CMPSTR (_mongoc_bson_type_to_str (result->value_type), "DOCUMENT");
   bson_init_from_value (&reply, result);
   bson_lookup_doc (&reply, "errorLabels", &labels);

   while (bson_iter_next (&expected_label)) {
      expected_label_str = bson_iter_utf8 (&expected_label, NULL);
      if (!_mongoc_bson_array_has_label (&labels, expected_label_str)) {
         test_error ("Expected label \"%s\" not found in %s",
                     expected_label_str,
                     bson_as_json (&labels, NULL));
      }
   }
}


static void
check_error_labels_omit (const bson_t *operation, const bson_value_t *result)
{
   bson_t reply;
   bson_t labels;
   bson_t omitted_labels;
   bson_iter_t omitted_label;

   if (!bson_has_field (operation, "result.errorLabelsOmit")) {
      return;
   }

   if (result->value_type != BSON_TYPE_DOCUMENT) {
      /* successful result from count, distinct, etc. */
      return;
   }

   bson_init_from_value (&reply, result);
   if (!bson_has_field (&reply, "errorLabels")) {
      return;
   }

   bson_lookup_doc (&reply, "errorLabels", &labels);
   bson_lookup_doc (operation, "result.errorLabelsOmit", &omitted_labels);
   BSON_ASSERT (bson_iter_init (&omitted_label, &omitted_labels));
   while (bson_iter_next (&omitted_label)) {
      if (_mongoc_bson_array_has_label (
             &labels, bson_iter_utf8 (&omitted_label, NULL))) {
         test_error ("Label \"%s\" should have been omitted %s",
                     bson_iter_utf8 (&omitted_label, NULL),
                     value_to_str (result));
      }
   }
}


/*--------------------------------------------------------------------------
 *
 * check_result --
 *
 *       Verify that a function call's outcome matches the expected outcome.
 *
 *       Consider a JSON test like:
 *
 *         operations:
 *           - name: insertOne
 *             arguments:
 *               document:
 *                 _id: 1
 *               session: session0
 *             result:
 *               insertedId: 1
 *
 *       @test is the BSON representation of the entire test including the
 *       "operations" array, @operation is one of the documents in array,
 *       @succeeded is true if the function call actually succeeded, @result
 *       is the function call's result (optional), and @error is the call's
 *       error (optional).
 *
 * Side effects:
 *       Logs and aborts if the outcome does not match the expected outcome.
 *
 *--------------------------------------------------------------------------
 */


static void
check_result (const bson_t *test,
              const bson_t *operation,
              bool succeeded,
              const bson_value_t *result,
              const bson_error_t *error)
{
   bson_value_t expected_result;
   char errmsg[1000];
   match_ctx_t ctx = {0};

   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof errmsg;

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

   /* if there's no "result", e.g. in the command monitoring tests, we don't
    * know if the command is expected to succeed or fail */
   if (!bson_has_field (operation, "result")) {
      return;
   }

   if (!bson_has_field (operation, "result.errorCodeName") &&
       !bson_has_field (operation, "result.errorContains") &&
       !bson_has_field (operation, "result.errorLabelsContain") &&
       !bson_has_field (operation, "result.errorLabelsOmit")) {
      /* expect the operation has succeeded */
      check_success_expected (operation, succeeded, true, error);
      if (!get_successful_result (test, operation, &expected_result)) {
         /* some tests don't verify the return value */
         return;
      }

      BSON_ASSERT (result);
      if (!match_bson_value (result, &expected_result, &ctx)) {
         test_error ("Error in \"%s\" test %s\n"
                     "Expected:\n%s\nActual:\n%s",
                     bson_lookup_utf8 (test, "description"),
                     ctx.errmsg,
                     value_to_str (&expected_result),
                     value_to_str (result));
      }

      bson_value_destroy (&expected_result);
      return;
   }

   /* transactions tests specify errors per-operation, with one or more details:
    *    operations:
    *      - name: insertOne
    *        arguments: ...
    *        result:
    *          errorCodeName: WriteConflict
    *          errorContains: "message substring"
    *          errorLabelsContain: ["TransientTransactionError"]
    *          errorLabelsOmit: ["UnknownTransactionCommitResult"]
    */

   check_success_expected (operation, succeeded, false, error);
   check_error_code_name (operation, error);
   check_error_contains (operation, error);
   check_error_labels_contain (operation, result);
   check_error_labels_omit (operation, result);
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


static void
execute_bulk_operation (mongoc_bulk_operation_t *bulk,
                        const bson_t *test,
                        const bson_t *operation)
{
   uint32_t server_id;
   bson_error_t error;
   bson_t reply;
   bson_value_t value;

   server_id = mongoc_bulk_operation_execute (bulk, &reply, &error);
   value_init_from_doc (&value, &reply);
   check_result (test, operation, server_id != 0, &value, &error);
   bson_value_destroy (&value);
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

   if (!mongoc_write_concern_is_default (wc)) {
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
   bson_value_t value = {0};
   bson_error_t error;
   bool r;

   name = bson_lookup_utf8 (operation, "name");
   bson_lookup_doc (operation, "arguments", &args);
   bson_lookup_doc (operation, "arguments.filter", &filter);

   opts = create_find_and_modify_opts (name, &args, session, wc);
   r = mongoc_collection_find_and_modify_with_opts (
      collection, &filter, opts, &reply, &error);


   /* Transactions Tests have findAndModify results like:
    *   result: {_id: 3}
    *
    * Or for findOneAndDelete with no result:
    *   result: null
    *
    * But mongoc_collection_find_and_modify_with_opts returns:
    *   { ok: 1, value: {_id: 3}}
    *
    * Or:
    *   { ok: 1, value: null}
    */
   if (r) {
      bson_lookup_value (&reply, "value", &value);
   }

   check_result (test, operation, r, &value, &error);

   bson_value_destroy (&value);
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
   bson_t reply = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   bson_error_t error;
   int64_t r;
   bson_value_t value;

   bson_lookup_doc (operation, "arguments.filter", &filter);
   append_session (session, &opts);
   r = mongoc_collection_count_with_opts (
      collection, MONGOC_QUERY_NONE, &filter, 0, 0, &opts, read_prefs, &error);

   if (r >= 0) {
      value.value_type = BSON_TYPE_INT64;
      value.value.v_int64 = r;
   } else {
      /* fake a reply for the test framework's sake */
      value_init_from_doc (&value, &reply);
   }

   check_result (test, operation, r > -1, &value, &error);

   bson_value_destroy (&value);
   bson_destroy (&reply);
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
   bson_value_t value = {0};
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

   /* Transactions Tests have "distinct" results like:
    *   result: [1, 2, 3]
    *
    * But the command returns:
    *   { ok: 1, values: [1, 2, 3]} */
   if (r) {
      bson_lookup_value (&reply, "values", &value);
   } else {
      value_init_from_doc (&value, &reply);
   }

   check_result (test, operation, r, &value, &error);

   bson_value_destroy (&value);
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
   bson_t result = BSON_INITIALIZER;
   const char *keyptr = NULL;
   char key[12];
   uint32_t i = 0;
   bson_value_t value;

   i = 0;
   while (mongoc_cursor_next (cursor, &doc)) {
      bson_uint32_to_string (i++, &keyptr, key, sizeof key);
      BSON_APPEND_DOCUMENT (&result, keyptr, doc);
   }

   if (mongoc_cursor_error_document (cursor, &error, &doc)) {
      value_init_from_doc (&value, doc);
      check_result (test, operation, false, &value, &error);
   } else {
      value_init_from_doc (&value, &result);
      value.value_type = BSON_TYPE_ARRAY;
      check_result (test, operation, true, &value, &error);
   }

   bson_value_destroy (&value);
   bson_destroy (&result);
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
command (mongoc_database_t *db,
         const bson_t *test,
         const bson_t *operation,
         mongoc_client_session_t *session,
         const mongoc_read_prefs_t *read_prefs)
{
   bson_t cmd;
   bson_t opts = BSON_INITIALIZER;
   bson_t reply;
   bson_error_t error;
   bool r;
   bson_value_t value;

   bson_lookup_doc (operation, "arguments.command", &cmd);
   append_session (session, &opts);

   r = mongoc_database_command_with_opts (
      db, &cmd, read_prefs, &opts, &reply, &error);

   value_init_from_doc (&value, &reply);
   check_result (test, operation, r, &value, &error);
   bson_value_destroy (&value);
   bson_destroy (&reply);
   bson_destroy (&opts);
   bson_destroy (&cmd);
}


static void
start_transaction (json_test_ctx_t *ctx,
                   const bson_t *test,
                   const bson_t *operation)
{
   mongoc_client_session_t *session;
   mongoc_transaction_opt_t *opts = NULL;
   bson_error_t error;
   bool r;

   session = session_from_name (ctx, bson_lookup_utf8 (operation, "object"));

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
commit_transaction (json_test_ctx_t *ctx,
                    const bson_t *test,
                    const bson_t *operation)
{
   mongoc_client_session_t *session;
   bson_t reply;
   bson_value_t value;
   bson_error_t error;
   bool r;

   session = session_from_name (ctx, bson_lookup_utf8 (operation, "object"));
   r = mongoc_client_session_commit_transaction (session, &reply, &error);
   value_init_from_doc (&value, &reply);
   check_result (test, operation, r, &value, &error);
   bson_value_destroy (&value);
   bson_destroy (&reply);
}


static void
abort_transaction (json_test_ctx_t *ctx,
                   const bson_t *test,
                   const bson_t *operation)
{
   mongoc_client_session_t *session;
   bson_t reply = BSON_INITIALIZER;
   bson_value_t value;
   bson_error_t error;
   bool r;

   session = session_from_name (ctx, bson_lookup_utf8 (operation, "object"));
   r = mongoc_client_session_abort_transaction (session, &error);
   /* fake a reply for the test framework's sake */
   value_init_from_doc (&value, &reply);
   check_result (test, operation, r, &value, &error);
   bson_value_destroy (&value);
   bson_destroy (&reply);
}


void
json_test_operation (json_test_ctx_t *ctx,
                     const bson_t *test,
                     const bson_t *operation,
                     mongoc_client_session_t *session)
{
   const char *op_name;
   mongoc_read_prefs_t *read_prefs = NULL;
   mongoc_write_concern_t *wc;
   mongoc_collection_t *collection;

   op_name = bson_lookup_utf8 (operation, "name");
   /* databaseOptions don't yet exist in tests, therefore not implemented */
   BSON_ASSERT (!bson_has_field (operation, "databaseOptions"));
   collection = mongoc_collection_copy (ctx->collection);
   if (bson_has_field (operation, "collectionOptions")) {
      bson_lookup_collection_opts (operation, "collectionOptions", collection);
   }

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
   } else if (!strcmp (op_name, "runCommand")) {
      command (ctx->db, test, operation, session, read_prefs);
   } else if (!strcmp (op_name, "startTransaction")) {
      start_transaction (ctx, test, operation);
   } else if (!strcmp (op_name, "commitTransaction")) {
      commit_transaction (ctx, test, operation);
   } else if (!strcmp (op_name, "abortTransaction")) {
      abort_transaction (ctx, test, operation);
   } else {
      test_error ("unrecognized operation name %s", op_name);
   }

   mongoc_collection_destroy (collection);
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_write_concern_destroy (wc);
}


static void
one_operation (json_test_ctx_t *ctx,
               const bson_t *test,
               const bson_t *operation)
{
   const char *op_name;
   mongoc_write_concern_t *wc = NULL;

   op_name = bson_lookup_utf8 (operation, "name");
   if (ctx->verbose) {
      printf ("     %s\n", op_name);
      fflush (stdout);
   }

   if (bson_has_field (operation, "arguments.writeConcern")) {
      wc = bson_lookup_write_concern (operation, "arguments.writeConcern");
   } else if (bson_has_field (operation, "collectionOptions.writeConcern")) {
      wc = bson_lookup_write_concern (operation,
                                      "collectionOptions.writeConcern");
   }

   if (wc) {
      ctx->acknowledged = mongoc_write_concern_is_acknowledged (wc);
      mongoc_write_concern_destroy (wc);
   } else {
      ctx->acknowledged = true;
   }

   if (ctx->config->run_operation_cb) {
      ctx->config->run_operation_cb (ctx, test, operation);
   } else {
      test_error ("set json_test_config_t.run_operation_cb to a callback"
                  " that executes json_test_operation()");
   }
}


void
json_test_operations (json_test_ctx_t *ctx, const bson_t *test)
{
   bson_t operations;
   bson_t operation;
   bson_iter_t iter;

   /* run each CRUD operation in the test, using the config's run-operation
    * callback, by default json_test_operation(). retryable writes tests have
    * one operation each, transactions tests have an array of them. */
   if (bson_has_field (test, "operation")) {
      bson_lookup_doc (test, "operation", &operation);
      one_operation (ctx, test, &operation);
   } else {
      bson_lookup_doc (test, "operations", &operations);
      ASSERT_CMPUINT32 (bson_count_keys (&operations), >, (uint32_t) 0);
      BSON_ASSERT (bson_iter_init (&iter, &operations));
      while (bson_iter_next (&iter)) {
         bson_iter_bson (&iter, &operation);
         one_operation (ctx, test, &operation);
      }
   }
}
