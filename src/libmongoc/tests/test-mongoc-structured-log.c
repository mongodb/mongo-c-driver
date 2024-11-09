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

#include <mongoc/mongoc.h>

#include "mongoc/mongoc-structured-log-private.h"
#include "TestSuite.h"

typedef struct log_assumption {
   mongoc_structured_log_envelope_t expected_envelope;
   bson_t *expected_bson;
   int expected_calls;
   int calls;
} log_assumption;

typedef struct structured_log_state {
   mongoc_structured_log_func_t handler;
   void *data;
} structured_log_state;

static BSON_INLINE structured_log_state
save_state (void)
{
   structured_log_state state;
   mongoc_structured_log_get_handler (&state.handler, &state.data);
   return state;
}

static BSON_INLINE void
restore_state (structured_log_state state)
{
   mongoc_structured_log_set_handler (state.handler, state.data);
}

static void
structured_log_func (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   struct log_assumption *assumption = (struct log_assumption *) user_data;

   assumption->calls++;

   ASSERT_CMPINT (assumption->calls, <=, assumption->expected_calls);

   ASSERT_CMPINT (entry->envelope.level, ==, assumption->expected_envelope.level);
   ASSERT_CMPINT (entry->envelope.component, ==, assumption->expected_envelope.component);
   ASSERT_CMPSTR (entry->envelope.message, assumption->expected_envelope.message);

   ASSERT_CMPINT (entry->envelope.level, ==, mongoc_structured_log_entry_get_level (entry));
   ASSERT_CMPINT (entry->envelope.component, ==, mongoc_structured_log_entry_get_component (entry));

   // Each call to message_as_bson allocates an identical copy
   bson_t *bson_1 = mongoc_structured_log_entry_message_as_bson (entry);
   bson_t *bson_2 = mongoc_structured_log_entry_message_as_bson (entry);

   // Compare for exact bson equality *after* comparing json strings, to give a more user friendly error on most
   // failures
   char *json_actual = bson_as_relaxed_extended_json (bson_1, NULL);
   char *json_expected = bson_as_relaxed_extended_json (assumption->expected_bson, NULL);
   ASSERT_CMPSTR (json_actual, json_expected);

   ASSERT (bson_equal (bson_1, assumption->expected_bson));
   ASSERT (bson_equal (bson_2, assumption->expected_bson));
   bson_destroy (bson_2);
   bson_destroy (bson_1);
   bson_free (json_actual);
   bson_free (json_expected);
}

void
test_plain_log_entry (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Plain log entry",
      .expected_bson = BCON_NEW ("message", BCON_UTF8 ("Plain log entry")),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_WARNING, MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND, "Plain log entry");

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_log_entry_with_extra_data (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Plain log entry",
      .expected_bson = BCON_NEW ("message", BCON_UTF8 ("Plain log entry"), "extra", BCON_INT32 (1)),
      .expected_calls = 1,
   };

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Plain log entry",
                          MONGOC_STRUCTURED_LOG_INT32 ("extra", 1));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
}

void
test_log_entry_with_all_data_types (void)
{
   struct log_assumption assumption = {
      .expected_envelope.level = MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      .expected_envelope.component = MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      .expected_envelope.message = "Log entry with all data types",
      .expected_bson = BCON_NEW ("message",
                                 BCON_UTF8 ("Log entry with all data types"),
                                 "k1",
                                 BCON_UTF8 ("string value"),
                                 "k2",
                                 BCON_NULL,
                                 "k3",
                                 BCON_INT32 (-12345),
                                 "k4",
                                 BCON_INT64 (0x76543210aabbccdd),
                                 "k5",
                                 BCON_BOOL (true),
                                 "k6",
                                 BCON_BOOL (false),
                                 "k7",
                                 BCON_UTF8 ("{ \"k\" : \"v\" }"),
                                 "k8",
                                 BCON_UTF8 ("112233445566778899aabbcc"),
                                 "databaseName",
                                 BCON_UTF8 ("Some database"),
                                 "commandName",
                                 BCON_UTF8 ("Not a command"),
                                 "operationId",
                                 BCON_INT64 (0x12345678eeff0011),
                                 "command",
                                 BCON_UTF8 ("{ \"c\" : \"d\" }"),
                                 "serverHost",
                                 BCON_UTF8 ("db.example.com"),
                                 "serverPort",
                                 BCON_INT32 (2345),
                                 "serverConnectionId",
                                 BCON_INT64 (0x3deeff0011223345),
                                 "serviceId",
                                 BCON_UTF8 ("2233445566778899aabbccdd")),
      .expected_calls = 1,
   };

   bson_t *json_doc = BCON_NEW ("k", BCON_UTF8 ("v"));
   bson_t *cmd_doc = BCON_NEW ("c", BCON_UTF8 ("d"));

   bson_oid_t oid;
   bson_oid_init_from_string (&oid, "112233445566778899aabbcc");

   mongoc_cmd_t cmd = {
      .db_name = "Some database",
      .command_name = "Not a command",
      .operation_id = 0x12345678eeff0011,
      .command = cmd_doc,
   };

   mongoc_server_description_t server_description = {
      .host.host = "db.example.com",
      .host.port = 2345,
      .server_connection_id = 0x3deeff0011223345,
   };
   bson_oid_init_from_string (&server_description.service_id, "2233445566778899aabbccdd");

   structured_log_state old_state = save_state ();
   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
      MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      "Log entry with all data types",
      // Basic BSON types.
      // Supports optional values (skip when key is NULL)
      MONGOC_STRUCTURED_LOG_UTF8 ("k1", "string value"),
      MONGOC_STRUCTURED_LOG_UTF8 ("k2", NULL),
      MONGOC_STRUCTURED_LOG_UTF8 (NULL, NULL),
      MONGOC_STRUCTURED_LOG_INT32 ("k3", -12345),
      MONGOC_STRUCTURED_LOG_INT32 (NULL, 9999),
      MONGOC_STRUCTURED_LOG_INT64 ("k4", 0x76543210aabbccdd),
      MONGOC_STRUCTURED_LOG_INT64 (NULL, -1),
      MONGOC_STRUCTURED_LOG_BOOL ("k5", true),
      MONGOC_STRUCTURED_LOG_BOOL ("k6", false),
      MONGOC_STRUCTURED_LOG_BOOL (NULL, true),
      // Deferred conversions
      MONGOC_STRUCTURED_LOG_BSON_AS_JSON ("k7", json_doc),
      MONGOC_STRUCTURED_LOG_BSON_AS_JSON (NULL, NULL),
      MONGOC_STRUCTURED_LOG_OID_AS_HEX ("k8", &oid),
      MONGOC_STRUCTURED_LOG_OID_AS_HEX (NULL, NULL),
      // Common structures, with explicit set of keys to include
      MONGOC_STRUCTURED_LOG_CMD (&cmd,
                                 (MONGOC_STRUCTURED_LOG_CMD_COMMAND | MONGOC_STRUCTURED_LOG_CMD_DATABASE_NAME |
                                  MONGOC_STRUCTURED_LOG_CMD_COMMAND_NAME | MONGOC_STRUCTURED_LOG_CMD_OPERATION_ID)),
      MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION (&server_description,
                                                (MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_HOST |
                                                 MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_PORT |
                                                 MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVER_CONNECTION_ID |
                                                 MONGOC_STRUCTURED_LOG_SERVER_DESCRIPTION_SERVICE_ID)));

   ASSERT_CMPINT (assumption.calls, ==, 1);
   restore_state (old_state);
   bson_destroy (assumption.expected_bson);
   bson_destroy (json_doc);
   bson_destroy (cmd_doc);
}

void
test_structured_log_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/structured_log/plain", test_plain_log_entry);
   TestSuite_Add (suite, "/structured_log/with_extra_data", test_log_entry_with_extra_data);
   TestSuite_Add (suite, "/structured_log/with_all_data_types", test_log_entry_with_all_data_types);
}
