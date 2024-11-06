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

struct log_assumption {
   mongoc_structured_log_entry_t expected_entry;
   int expected_calls;
   int calls;
};

struct structured_log_state {
   mongoc_structured_log_func_t handler;
   void *data;
};

static void
save_state (struct structured_log_state *state)
{
   _mongoc_structured_log_get_handler (&state->handler, &state->data);
}

static void
restore_state (const struct structured_log_state *state)
{
   mongoc_structured_log_set_handler (state->handler, state->data);
}

static void
structured_log_func (mongoc_structured_log_entry_t *entry, void *user_data)
{
   struct log_assumption *assumption = (struct log_assumption *) user_data;

   assumption->calls++;

   ASSERT_CMPINT (assumption->calls, <=, assumption->expected_calls);

   ASSERT_CMPINT (entry->level, ==, assumption->expected_entry.level);
   ASSERT_CMPINT (entry->component, ==, assumption->expected_entry.component);
   ASSERT (bson_equal (mongoc_structured_log_entry_get_message (entry), assumption->expected_entry.structured_message));
}

void
test_plain_log_entry ()
{
   struct structured_log_state old_state;
   struct log_assumption assumption = {
      {
         MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
         MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
         "Plain log entry",
         BCON_NEW ("message", BCON_UTF8 ("Plain log entry")),
      },
      1,
      0,
   };

   save_state (&old_state);

   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (
      MONGOC_STRUCTURED_LOG_LEVEL_WARNING, MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND, "Plain log entry", NULL, NULL);

   ASSERT_CMPINT (assumption.calls, =, 1);

   restore_state (&old_state);

   bson_destroy (assumption.expected_entry.structured_message);
}

void
_test_append_extra_data (mongoc_structured_log_component_t component,
                         void *structured_log_data,
                         bson_t *structured_message /* OUT */)
{
   BCON_APPEND (structured_message, "extra", BCON_INT32 (1));
}

void
test_log_entry_with_extra_data ()
{
   struct structured_log_state old_state;
   struct log_assumption assumption = {
      {
         MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
         MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
         "Plain log entry",
         BCON_NEW ("message", BCON_UTF8 ("Plain log entry"), "extra", BCON_INT32 (1)),
      },
      1,
      0,
   };

   save_state (&old_state);

   mongoc_structured_log_set_handler (structured_log_func, &assumption);

   mongoc_structured_log (MONGOC_STRUCTURED_LOG_LEVEL_WARNING,
                          MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
                          "Plain log entry",
                          _test_append_extra_data,
                          NULL);

   ASSERT_CMPINT (assumption.calls, =, 1);

   restore_state (&old_state);

   bson_destroy (assumption.expected_entry.structured_message);
}

void
test_structured_log_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/structured_log/plain", test_plain_log_entry);
   TestSuite_Add (suite, "/structured_log/with_extra_data", test_log_entry_with_extra_data);
}
