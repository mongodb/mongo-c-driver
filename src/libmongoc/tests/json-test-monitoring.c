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


#include "mongoc-config.h"
#include "mongoc-collection-private.h"
#include "mongoc-host-list-private.h"
#include "mongoc-server-description-private.h"
#include "mongoc-topology-description-private.h"
#include "mongoc-topology-private.h"
#include "mongoc-util-private.h"
#include "mongoc-util-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"

#include "json-test.h"
#include "json-test-operations.h"
#include "test-libmongoc.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#endif

/* replace a real cursor id with what JSON tests expect: 42 for a live cursor,
 * 0 for a dead one */
static int64_t
fake_cursor_id (const bson_iter_t *iter)
{
   return bson_iter_as_int64 (iter) ? 42 : 0;
}


static bool
ends_with (const char *s, const char *suffix)
{
   size_t s_len;
   size_t suffix_len;

   if (!s) {
      return false;
   }

   s_len = strlen (s);
   suffix_len = strlen (suffix);
   return s_len >= suffix_len && !strcmp (s + s_len - suffix_len, suffix);
}


static bool
lsids_match (const bson_t *a, const bson_t *b)
{
   /* need a match context in case lsids DON'T match, since match_bson() without
    * context aborts on mismatch */
   char errmsg[1000];
   match_ctx_t ctx = {0};
   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof (errmsg);

   return match_bson_with_ctx (a, b, false, &ctx);
}


/* Convert "ok" values to doubles, cursor ids and error codes to 42, and
 * error messages to "". See README at
 * github.com/mongodb/specifications/tree/master/source/command-monitoring/tests
 */
static void
convert_message_for_test (json_test_ctx_t *ctx,
                          const bson_t *src,
                          bson_t *dst,
                          const char *path)
{
   bson_iter_t iter;
   const char *key;
   const char *errmsg;
   bson_t src_child;
   bson_t dst_child;
   char *child_path;
   bson_t lsid;

   if (bson_empty (src) && !ctx->acknowledged) {
      /* spec tests say unacknowledged writes reply "ok": 1, but we don't */
      BSON_APPEND_DOUBLE (dst, "ok", 1.0);
      return;
   }

   if (!path && !bson_empty (src)) {
      const char *cmd_name = _mongoc_get_command_name (src);
      if (!strcmp (cmd_name, "find") || !strcmp (cmd_name, "aggregate")) {
         /* New query. Next server reply or getMore will set cursor_id. */
         ctx->cursor_id = 0;
      }
   }

   BSON_ASSERT (bson_iter_init (&iter, src));

   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);

      if (!strcmp (key, "ok")) {
         /* "The server is inconsistent on whether the ok values returned are
          * integers or doubles so for simplicity the tests specify all expected
          * values as doubles. Server 'ok' values of integers MUST be converted
          * to doubles for comparison with the expected values."
          */
         BSON_APPEND_DOUBLE (dst, key, (double) bson_iter_as_int64 (&iter));

      } else if (!strcmp (key, "errmsg")) {
         /* "errmsg values of "" MUST assert that the value is not empty" */
         errmsg = bson_iter_utf8 (&iter, NULL);
         ASSERT_CMPSIZE_T (strlen (errmsg), >, (size_t) 0);
         BSON_APPEND_UTF8 (dst, key, "");

      } else if (!strcmp (key, "id") && ends_with (path, "cursor")) {
         /* store find/aggregate reply's cursor id, replace with 42 or 0 */
         ctx->cursor_id = bson_iter_int64 (&iter);
         BSON_APPEND_INT64 (dst, key, fake_cursor_id (&iter));

      } else if (ends_with (path, "cursors") ||
                 ends_with (path, "cursorsUnknown")) {
         /* payload of a killCursors command-started event:
          *    {killCursors: "test", cursors: [12345]}
          * or killCursors command-succeeded event:
          *    {ok: 1, cursorsUnknown: [12345]}
          * */
         ASSERT_CMPINT64 (bson_iter_as_int64 (&iter), >, (int64_t) 0);
         BSON_APPEND_INT64 (dst, key, 42);

      } else if (!strcmp (key, "getMore")) {
         /* "When encountering a cursor or getMore value of "42" in a test, the
          * driver MUST assert that the values are equal to each other and
          * greater than zero."
          */
         if (ctx->cursor_id == 0) {
            ctx->cursor_id = bson_iter_int64 (&iter);
         } else {
            ASSERT_CMPINT64 (ctx->cursor_id, ==, bson_iter_int64 (&iter));
         }

         BSON_APPEND_INT64 (dst, key, fake_cursor_id (&iter));

      } else if (!strcmp (key, "code")) {
         /* "code values of 42 MUST assert that the value is present and
          * greater than zero" */
         ASSERT_CMPINT64 (bson_iter_as_int64 (&iter), >, (int64_t) 0);
         BSON_APPEND_INT32 (dst, key, 42);

      } else if (!strcmp (key, "lsid") && BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         /* Transactions tests: "Each command-started event in "expectations"
          * includes an lsid with the value "session0" or "session1". Tests MUST
          * assert that the command's actual lsid matches the id of the correct
          * ClientSession named session0 or session1." */
         bson_iter_bson (&iter, &lsid);
         if (lsids_match (&ctx->lsids[0], &lsid)) {
            BSON_APPEND_UTF8 (dst, key, "session0");
         } else if (lsids_match (&ctx->lsids[1], &lsid)) {
            BSON_APPEND_UTF8 (dst, key, "session1");
         }

      } else if (!strcmp (key, "afterClusterTime") &&
                 BSON_ITER_HOLDS_TIMESTAMP (&iter) && path &&
                 !strcmp (path, "readConcern")) {
         /* Transactions tests: "A readConcern.afterClusterTime value of 42 in
          * a command-started event is a fake cluster time. Drivers MUST assert
          * that the actual command includes an afterClusterTime." */
         BSON_APPEND_INT32 (dst, key, 42);

      } else if (BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         if (path) {
            child_path = bson_strdup_printf ("%s.%s", path, key);
         } else {
            child_path = bson_strdup (key);
         }

         bson_iter_bson (&iter, &src_child);
         bson_append_document_begin (dst, key, -1, &dst_child);
         convert_message_for_test (
            ctx, &src_child, &dst_child, child_path); /* recurse */
         bson_append_document_end (dst, &dst_child);
         bson_free (child_path);
      } else if (BSON_ITER_HOLDS_ARRAY (&iter)) {
         if (path) {
            child_path = bson_strdup_printf ("%s.%s", path, key);
         } else {
            child_path = bson_strdup (key);
         }

         bson_iter_bson (&iter, &src_child);
         bson_append_array_begin (dst, key, -1, &dst_child);
         convert_message_for_test (
            ctx, &src_child, &dst_child, child_path); /* recurse */
         bson_append_array_end (dst, &dst_child);
         bson_free (child_path);
      } else {
         bson_append_value (dst, key, -1, bson_iter_value (&iter));
      }
   }

   /* transaction tests expect "new: false" explicitly; we don't send it */
   if (!bson_empty (src) &&
       !strcmp ("findAndModify", _mongoc_get_command_name (src)) &&
       !bson_has_field (src, "new")) {
      bson_append_bool (dst, "new", 3, false);
   }

   /* transaction tests expect "multi: false" and "upsert: false" explicitly;
    * we don't send them. fix when path is like "updates.0", "updates.1", ... */
   if (path && strstr (path, "updates.") == path) {
      const char *suffix = strchr (path, '.') + 1;
      if (isdigit (suffix[0])) {
         if (!bson_has_field (src, "multi")) {
            BSON_APPEND_BOOL (dst, "multi", false);
         }
         if (!bson_has_field (src, "upsert")) {
            BSON_APPEND_BOOL (dst, "upsert", false);
         }
      }
   }
}

/* test that an event's "host" field is set to a reasonable value */
static void
assert_host_in_uri (const mongoc_host_list_t *host, const mongoc_uri_t *uri)
{
   const mongoc_host_list_t *hosts;

   hosts = mongoc_uri_get_hosts (uri);
   while (hosts) {
      if (_mongoc_host_list_equal (hosts, host)) {
         return;
      }

      hosts = hosts->next;
   }

   fprintf (stderr,
            "Host \"%s\" not in \"%s\"",
            host->host_and_port,
            mongoc_uri_get_string (uri));
   fflush (stderr);
   abort ();
}


static void
started_cb (const mongoc_apm_command_started_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_started_get_context (event);
   char *cmd_json;
   bson_t *events = &ctx->events;
   bson_t cmd = BSON_INITIALIZER;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      cmd_json = bson_as_canonical_extended_json (event->command, NULL);
      printf ("%s\n", cmd_json);
      fflush (stdout);
      bson_free (cmd_json);
   }

   BSON_ASSERT (mongoc_apm_command_started_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_started_get_server_id (event) > 0);
   /* check that event->host is sane */
   assert_host_in_uri (event->host, ctx->test_framework_uri);
   convert_message_for_test (ctx, event->command, &cmd, NULL);
   new_event = BCON_NEW ("command_started_event",
                         "{",
                         "command",
                         BCON_DOCUMENT (&cmd),
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "database_name",
                         BCON_UTF8 (event->database_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
   bson_destroy (&cmd);
}


static void
succeeded_cb (const mongoc_apm_command_succeeded_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_succeeded_get_context (event);
   char *reply_json;
   bson_t reply = BSON_INITIALIZER;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      reply_json = bson_as_canonical_extended_json (event->reply, NULL);
      printf ("\t\t<-- %s\n", reply_json);
      fflush (stdout);
      bson_free (reply_json);
   }

   BSON_ASSERT (mongoc_apm_command_succeeded_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_succeeded_get_server_id (event) > 0);
   assert_host_in_uri (event->host, ctx->test_framework_uri);
   convert_message_for_test (ctx, event->reply, &reply, NULL);
   new_event = BCON_NEW ("command_succeeded_event",
                         "{",
                         "reply",
                         BCON_DOCUMENT (&reply),
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (&ctx->events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
   bson_destroy (&reply);
}


static void
failed_cb (const mongoc_apm_command_failed_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_failed_get_context (event);
   bson_t reply = BSON_INITIALIZER;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      printf (
         "\t\t<-- %s FAILED: %s\n", event->command_name, event->error->message);
      fflush (stdout);
   }

   BSON_ASSERT (mongoc_apm_command_failed_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_failed_get_server_id (event) > 0);
   assert_host_in_uri (event->host, ctx->test_framework_uri);

   new_event = BCON_NEW ("command_failed_event",
                         "{",
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (&ctx->events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
   bson_destroy (&reply);
}


void
set_apm_callbacks (mongoc_client_t *client,
                   bool command_started_events_only,
                   void *ctx)
{
   mongoc_apm_callbacks_t *callbacks;

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, started_cb);

   if (!command_started_events_only) {
      mongoc_apm_set_command_succeeded_cb (callbacks, succeeded_cb);
      mongoc_apm_set_command_failed_cb (callbacks, failed_cb);
   }

   mongoc_client_set_apm_callbacks (client, callbacks, ctx);
   mongoc_apm_callbacks_destroy (callbacks);
}


/*
 *-----------------------------------------------------------------------
 *
 * check_json_apm_events --
 *
 *      Compare actual APM events with expected sequence. The two docs
 *      are each like:
 *
 * [
 *   {
 *     "command_started_event": {
 *       "command": { ... },
 *       "command_name": "count",
 *       "database_name": "command-monitoring-tests",
 *       "operation_id": 123
 *     }
 *   },
 *   {
 *     "command_failed_event": {
 *       "command_name": "count",
 *       "operation_id": 123
 *     }
 *   }
 * ]
 *
 *      If @allow_subset is true, then expectations is allowed to be
 *      a subset of events.
 *
 *-----------------------------------------------------------------------
 */
void
check_json_apm_events (const bson_t *events,
                       const bson_t *expectations,
                       bool allow_subset)
{
   char errmsg[1000] = {0};
   match_ctx_t ctx = {0};
   uint32_t expected_keys;
   uint32_t actual_keys;

   /* Old mongod returns a double for "count", newer returns int32.
    * Ignore this and other insignificant type differences. */
   ctx.strict_numeric_types = false;
   ctx.retain_dots_in_keys = true;
   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof errmsg;

   if (!allow_subset) {
      expected_keys = bson_count_keys (expectations);
      actual_keys = bson_count_keys (events);

      if (expected_keys != actual_keys) {
         test_error ("command monitoring test failed expectations:\n\n"
                     "%s\n\n"
                     "events:\n%s\n\n"
                     "expected %" PRIu32 " events, got %" PRIu32,
                     bson_as_canonical_extended_json (expectations, NULL),
                     bson_as_canonical_extended_json (events, NULL),
                     expected_keys,
                     actual_keys);

         abort ();
      }

      if (!match_bson_with_ctx (events, expectations, false, &ctx)) {
         test_error ("command monitoring test failed expectations:\n\n"
                     "%s\n\n"
                     "events:\n%s\n\n%s",
                     bson_as_canonical_extended_json (expectations, NULL),
                     bson_as_canonical_extended_json (events, NULL),
                     errmsg);
      }
   } else {
      bson_iter_t expectations_iter;
      BSON_ASSERT (bson_iter_init (&expectations_iter, expectations));

      while (bson_iter_next (&expectations_iter)) {
         bson_t expectation;
         bson_iter_bson (&expectations_iter, &expectation);
         match_in_array (&expectation, events, &ctx);
         bson_destroy (&expectation);
      }
   }
}
