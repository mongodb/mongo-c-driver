/*
 * Copyright 2019-present MongoDB, Inc.
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

#include "json-test.h"
#include "test-libmongoc.h"

static void
_before_test (json_test_ctx_t *ctx, const bson_t *test)
{
   mongoc_client_t *client;
   mongoc_collection_t *key_vault_coll;
   bson_iter_t iter;
   bson_error_t error;
   bool ret;
   mongoc_write_concern_t *wc;
   bson_t insert_opts;

   /* Insert data into the key vault. */
   client = test_framework_client_new ();
   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, 1000);
   bson_init (&insert_opts);
   mongoc_write_concern_append (wc, &insert_opts);

   if (bson_iter_init_find (&iter, ctx->config->scenario, "key_vault_data")) {
      key_vault_coll =
         mongoc_client_get_collection (client, "admin", "datakeys");

      /* Drop and recreate, inserting data. */
      ret = mongoc_collection_drop (key_vault_coll, &error);
      if (!ret) {
         /* Ignore "namespace does not exist" error. */
         ASSERT_OR_PRINT (error.code == 26, error);
      }

      bson_iter_recurse (&iter, &iter);
      while (bson_iter_next (&iter)) {
         bson_t doc;

         bson_iter_bson (&iter, &doc);
         ret = mongoc_collection_insert_one (
            key_vault_coll, &doc, &insert_opts, NULL /* reply */, &error);
         ASSERT_OR_PRINT (ret, error);
      }
      mongoc_collection_destroy (key_vault_coll);
   }

   /* Collmod to include the json schema. Data was already inserted. */
   if (bson_iter_init_find (&iter, ctx->config->scenario, "json_schema")) {
      bson_t *cmd;
      bson_t json_schema;

      bson_iter_bson (&iter, &json_schema);
      cmd = BCON_NEW ("collMod",
                      BCON_UTF8 (mongoc_collection_get_name (ctx->collection)),
                      "validator",
                      "{",
                      "$jsonSchema",
                      BCON_DOCUMENT (&json_schema),
                      "}");
      ret = mongoc_client_command_simple (
         client, mongoc_database_get_name (ctx->db), cmd, NULL, NULL, &error);
      ASSERT_OR_PRINT (ret, error);
      bson_destroy (cmd);
   }

   bson_destroy (&insert_opts);
   mongoc_write_concern_destroy (wc);
   mongoc_client_destroy (client);
}

static bool
_run_operation (json_test_ctx_t *ctx,
                const bson_t *test,
                const bson_t *operation)
{
   bson_t reply;
   bool res;

   res =
      json_test_operation (ctx, test, operation, ctx->collection, NULL, &reply);

   bson_destroy (&reply);

   return res;
}

static void
test_client_side_encryption_cb (bson_t *scenario)
{
   json_test_config_t config = JSON_TEST_CONFIG_INIT;
   config.before_test_cb = _before_test;
   config.run_operation_cb = _run_operation;
   config.scenario = scenario;
   config.command_started_events_only = true;
   config.command_monitoring_allow_subset = false;
   run_json_general_test (&config);
}

void
test_client_side_encryption_install (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/client_side_encryption", resolved));
   install_json_test_suite_with_check (
      suite,
      resolved,
      test_client_side_encryption_cb,
      test_framework_skip_if_no_client_side_encryption);
}