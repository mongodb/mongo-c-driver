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

#include "json-test-operations.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"


void
json_test_ctx_init (json_test_ctx_t *ctx)
{
   ctx->n_events = 0;
   bson_init (&ctx->events);
   ctx->test_framework_uri = test_framework_get_uri ();
   ctx->cursor_id = 0;
   ctx->acknowledged = true;
   ctx->verbose = test_framework_getenv_bool ("MONGOC_TEST_MONITORING_VERBOSE");
}


void
json_test_ctx_cleanup (json_test_ctx_t *ctx)
{
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
add_request_to_bulk (mongoc_bulk_operation_t *bulk, bson_t *request)
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

   bson_destroy (&args);
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
execute_bulk_operation (mongoc_bulk_operation_t *bulk, const bson_t *test)
{
   uint32_t server_id;
   bson_t reply;
   bson_error_t error;

   server_id = mongoc_bulk_operation_execute (bulk, &reply, &error);

   if (_mongoc_lookup_bool (test, "outcome.error", false)) {
      ASSERT (!server_id);
   } else if (bson_has_field (test, "outcome.result")) {
      ASSERT_OR_PRINT (server_id, error);
      bson_t spec_result;
      bson_t *expected_result;

      bson_lookup_doc (test, "outcome.result", &spec_result);
      expected_result = convert_spec_result_to_bulk_write_result (&spec_result);

      ASSERT (match_bson (&reply, expected_result, false));
   }

   bson_destroy (&reply);
}

static bson_t *
create_bulk_write_opts (const bson_t *test,
                        mongoc_client_session_t *session,
                        mongoc_write_concern_t *wc)
{
   bson_t *opts;
   bson_t tmp;

   opts = tmp_bson ("{}");

   if (bson_has_field (test, "operation.arguments.options")) {
      bson_lookup_doc (test, "operation.arguments.options", &tmp);
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
            mongoc_client_session_t *session,
            mongoc_write_concern_t *wc)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t requests;
   bson_iter_t iter;

   opts = create_bulk_write_opts (test, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation.arguments.requests", &requests);
   ASSERT (bson_iter_init (&iter, &requests));

   while (bson_iter_next (&iter)) {
      bson_t request;

      bson_iter_bson (&iter, &request);
      add_request_to_bulk (bulk, &request);
   }

   bson_destroy (&requests);
   execute_bulk_operation (bulk, test);
   mongoc_bulk_operation_destroy (bulk);
}


static void
single_write (mongoc_collection_t *collection,
              const bson_t *test,
              mongoc_client_session_t *session,
              mongoc_write_concern_t *wc)
{
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_t operation;

   /* for ease, use bulk for all writes, not mongoc_collection_insert_one etc */
   opts = create_bulk_write_opts (test, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation", &operation);
   add_request_to_bulk (bulk, &operation);

   execute_bulk_operation (bulk, test);

   bson_destroy (&operation);
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

   if (wc) {
      BSON_ASSERT (mongoc_write_concern_append (wc, &extra));
   }

   ASSERT (mongoc_find_and_modify_opts_append (opts, &extra));
   bson_destroy (&extra);

   return opts;
}


static void
find_and_modify (mongoc_collection_t *collection,
                 const bson_t *test,
                 mongoc_client_session_t *session,
                 mongoc_write_concern_t *wc)
{
   const char *name;
   bson_t args;
   bson_t filter;
   mongoc_find_and_modify_opts_t *opts;
   bool r;
   bson_t reply;
   bson_error_t error;

   name = bson_lookup_utf8 (test, "operation.name");
   bson_lookup_doc (test, "operation.arguments", &args);
   bson_lookup_doc (test, "operation.arguments.filter", &filter);

   opts = create_find_and_modify_opts (name, &args, session, wc);
   r = mongoc_collection_find_and_modify_with_opts (
      collection, &filter, opts, &reply, &error);
   mongoc_find_and_modify_opts_destroy (opts);

   if (_mongoc_lookup_bool (test, "outcome.error", false)) {
      ASSERT (!r);
   } else {
      ASSERT_OR_PRINT (r, error);
   }

   if (bson_has_field (test, "outcome.result")) {
      bson_t expected_result;
      bson_t reply_result;

      bson_lookup_doc (test, "outcome.result", &expected_result);
      bson_lookup_doc (&reply, "value", &reply_result);

      ASSERT (match_bson (&reply_result, &expected_result, false));
   }

   bson_destroy (&args);
   bson_destroy (&filter);
   bson_destroy (&reply);
}


static void
insert_many (mongoc_collection_t *collection,
             const bson_t *test,
             mongoc_client_session_t *session,
             mongoc_write_concern_t *wc)
{
   bson_t documents;
   bson_t *opts;
   mongoc_bulk_operation_t *bulk;
   bson_iter_t iter;

   opts = create_bulk_write_opts (test, session, wc);
   bulk = mongoc_collection_create_bulk_operation_with_opts (collection, opts);

   bson_lookup_doc (test, "operation.arguments.documents", &documents);
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

   execute_bulk_operation (bulk, test);

   bson_destroy (&documents);
   mongoc_bulk_operation_destroy (bulk);
}


static void
count (mongoc_collection_t *collection,
       const bson_t *test,
       mongoc_client_session_t *session,
       const mongoc_read_prefs_t *read_prefs)
{
   bson_t filter;
   bson_t opts = BSON_INITIALIZER;

   bson_lookup_doc (test, "operation.arguments.filter", &filter);
   append_session (session, &opts);
   mongoc_collection_count_with_opts (
      collection, MONGOC_QUERY_NONE, &filter, 0, 0, NULL, read_prefs, NULL);
}


static void
find (mongoc_collection_t *collection,
      const bson_t *test,
      mongoc_client_session_t *session,
      const mongoc_read_prefs_t *read_prefs)
{
   bson_t arguments;
   bson_t filter;
   bson_t opts = BSON_INITIALIZER;
   mongoc_cursor_t *cursor;
   const bson_t *doc;

   bson_lookup_doc (test, "operation.arguments", &arguments);
   bson_lookup_doc (&arguments, "filter", &filter);

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
      bson_error_t error;
      bool r;

      bson_lookup_doc (&arguments, "modifiers", &modifiers);
      bson_concat (query, &modifiers);
      r = _mongoc_cursor_translate_dollar_query_opts (
         query, &opts, &unwrapped, &error);
      ASSERT_OR_PRINT (r, error);
      bson_destroy (&unwrapped);
   }

   bson_copy_to_excluding_noinit (
      &arguments, &opts, "filter", "modifiers", NULL);

   append_session (session, &opts);

   cursor =
      mongoc_collection_find_with_opts (collection, &filter, &opts, read_prefs);

   while (mongoc_cursor_next (cursor, &doc)) {
   }

   /* can cause a killCursors command */
   mongoc_cursor_destroy (cursor);
   bson_destroy (&opts);
}


void
json_test_operation (json_test_ctx_t *ctx,
                     const bson_t *test,
                     mongoc_collection_t *collection,
                     mongoc_client_session_t *session)
{
   bson_t operation;
   bson_t tmp_args;
   bson_t args;
   bson_t tmp_opts;
   bson_t opts;
   const char *op_name;
   mongoc_read_prefs_t *read_prefs = NULL;
   mongoc_write_concern_t *wc;

   bson_lookup_doc (test, "operation", &operation);
   op_name = bson_lookup_utf8 (&operation, "name");
   bson_lookup_doc (&operation, "arguments", &tmp_args);

   if (bson_has_field (&tmp_args, "options")) {
      bson_lookup_doc (&tmp_args, "options", &tmp_opts);
   } else {
      bson_init (&tmp_opts);
   }

   /* make a copy of "options" and maybe append the sessionId */
   bson_copy_to (&tmp_opts, &opts);
   append_session (session, &opts);

   if (bson_has_field (&operation, "read_preference")) {
      read_prefs = bson_lookup_read_prefs (&operation, "read_preference");
   }

   if (bson_has_field (&operation, "arguments.writeConcern")) {
      wc = bson_lookup_write_concern (&operation, "arguments.writeConcern");
   } else {
      wc = mongoc_write_concern_new ();
   }

   ctx->acknowledged = mongoc_write_concern_is_acknowledged (wc);

   /* arguments besides "options" / "read_preference" are passed to commands */
   bson_init (&args);
   bson_copy_to_excluding_noinit (
      &tmp_args, &args, "options", "read_preference", NULL);

   if (!strcmp (op_name, "bulkWrite")) {
      bulk_write (collection, test, session, wc);
   } else if (!strcmp (op_name, "deleteOne") ||
              !strcmp (op_name, "deleteMany") ||
              !strcmp (op_name, "insertOne") ||
              !strcmp (op_name, "replaceOne") ||
              !strcmp (op_name, "updateOne") ||
              !strcmp (op_name, "updateMany")) {
      single_write (collection, test, session, wc);
   } else if (!strcmp (op_name, "findOneAndDelete") ||
              !strcmp (op_name, "findOneAndReplace") ||
              !strcmp (op_name, "findOneAndUpdate")) {
      find_and_modify (collection, test, session, wc);
   } else if (!strcmp (op_name, "insertMany")) {
      insert_many (collection, test, session, wc);
   } else if (!strcmp (op_name, "count")) {
      count (collection, test, session, read_prefs);
   } else if (!strcmp (op_name, "find")) {
      find (collection, test, session, read_prefs);
   } else {
      test_error ("unrecognized operation name %s", op_name);
      abort ();
   }

   bson_destroy (&args);
   bson_destroy (&tmp_opts);
   bson_destroy (&opts);
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_write_concern_destroy (wc);
}
