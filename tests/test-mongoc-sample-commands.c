/*
 * Copyright 2017 MongoDB, Inc.
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

/* MongoDB documentation examples
 *
 * One page on the MongoDB docs site shows a set of common tasks, with example
 * code for each driver plus the mongo shell. The source files for these code
 * examples are delimited with "Start Example 1" / "End Example 1" and so on.
 *
 * These are the C examples for that page.
 */

#include <mongoc.h>

#include "TestSuite.h"
#include "test-libmongoc.h"


typedef void (*sample_command_fn_t) (mongoc_client_t *client,
                                     mongoc_database_t *db);


static void
test_sample_command (void *ctx)
{
   sample_command_fn_t fn = (sample_command_fn_t) ctx;
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *collection;

   client = test_framework_client_new ();
   db = mongoc_client_get_database (client, "test_sample_command");
   collection = mongoc_database_get_collection (db, "inventory");
   mongoc_collection_drop (collection, NULL);

   fn (client, db);

   mongoc_collection_drop (collection, NULL);
   mongoc_collection_destroy (collection);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);
}


static void
test_example_one (mongoc_client_t *client, mongoc_database_t *db)
{
   /* Start Example 1 */
   mongoc_collection_t *collection;
   bson_t *doc;
   bson_error_t error;
   bool r;

   collection = mongoc_database_get_collection (db, "inventory");
   doc = BCON_NEW ("item",
                   BCON_UTF8 ("canvas"),
                   "qty",
                   BCON_INT64 (100),
                   "tags",
                   "[", BCON_UTF8 ("cotton"), "]", "size",
                   "{",
                   "h",
                   BCON_INT64 (28),
                   "w",
                   BCON_DOUBLE (35.5),
                   "uom",
                   BCON_UTF8 ("cm"),
                   "}");

   r = mongoc_collection_insert (
      collection, MONGOC_INSERT_NONE, doc, NULL, &error);

   if (!r) {
      MONGOC_ERROR ("%s\n", error.message);
   }
   /* End Example 1 */
   ASSERT_COUNT (1, collection);
   /* Start Example 1 post */
   bson_destroy (doc);
   mongoc_collection_destroy (collection);
   /* End Example 1 post */
}


static void
test_example_two (mongoc_client_t *client, mongoc_database_t *db)
{
   /* Start Example 2 */
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   bson_t *filter;

   collection = mongoc_database_get_collection (db, "inventory");
   filter = BCON_NEW ("item", BCON_UTF8 ("canvas"));
   cursor = mongoc_collection_find_with_opts (collection, filter, NULL, NULL);

   bson_destroy (filter);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   /* End Example 2 */
}


static void
test_example_three (mongoc_client_t *client, mongoc_database_t *db)
{
   /* Start Example 3 */
   mongoc_collection_t *collection;
   bson_t *doc;
   bson_error_t error;
   bool r;
   mongoc_bulk_operation_t *bulk;
   bson_t reply;

   collection = mongoc_database_get_collection (db, "inventory");
   bulk = mongoc_collection_create_bulk_operation (collection,
                                                   true /* ordered */,
                                                   NULL /* write concern */);

   doc = BCON_NEW ("item", BCON_UTF8 ("journal"),
                   "qty", BCON_INT64 (25),
                   "tags", "[", BCON_UTF8 ("blank"), BCON_UTF8 ("red"), "]",
                   "size", "{",
                   "h", BCON_INT64 (14),
                   "w", BCON_INT64 (21),
                   "uom", BCON_UTF8 ("cm"),
                   "}");

   r = mongoc_bulk_operation_insert_with_opts (bulk, doc, NULL /* opts */, &error);
   bson_destroy (doc);
   if (!r) {
      MONGOC_ERROR ("%s\n", error.message);
      goto done;
   }

   doc = BCON_NEW ("item", BCON_UTF8 ("mat"),
                   "qty", BCON_INT64 (85),
                   "tags", "[", BCON_UTF8 ("gray"), "]",
                   "size", "{",
                   "h", BCON_DOUBLE (27.9),
                   "w", BCON_DOUBLE (35.5),
                   "uom", BCON_UTF8 ("cm"),
                   "}");

   r = mongoc_bulk_operation_insert_with_opts (bulk, doc, NULL /* opts */, &error);
   bson_destroy (doc);
   if (!r) {
      MONGOC_ERROR ("%s\n", error.message);
      goto done;
   }

   doc = BCON_NEW ("item", BCON_UTF8 ("mousepad"),
                   "qty", BCON_INT64 (25),
                   "tags", "[", BCON_UTF8 ("gel"), BCON_UTF8 ("blue"), "]",
                   "size", "{",
                   "h", BCON_INT64 (19),
                   "w", BCON_DOUBLE (22.85),
                   "uom", BCON_UTF8 ("cm"),
                   "}");

   r = mongoc_bulk_operation_insert_with_opts (bulk, doc, NULL /* opts */,
                                               &error);
   bson_destroy (doc);
   if (!r) {
      MONGOC_ERROR ("%s\n", error.message);
      goto done;
   }

   /* "reply" is initialized on success or error */
   r = (bool) mongoc_bulk_operation_execute (bulk, &reply, &error);
   if (!r) {
      MONGOC_ERROR ("%s\n", error.message);
   }

   /* End Example 3 */
   ASSERT_COUNT (3, collection);
   /* Start Example 3 post */
   bson_destroy (&reply);
done:
   mongoc_collection_destroy (collection);
   mongoc_bulk_operation_destroy (bulk);
   /* End Example 3 post */
}


void
test_samples_install (TestSuite *suite)
{
   TestSuite_AddFull (suite,
                      "/Samples/one",
                      test_sample_command,
                      NULL,
                      test_example_one,
                      TestSuite_CheckLive);
   TestSuite_AddFull (suite,
                      "/Samples/two",
                      test_sample_command,
                      NULL,
                      test_example_two,
                      TestSuite_CheckLive);
   TestSuite_AddFull (suite,
                      "/Samples/three",
                      test_sample_command,
                      NULL,
                      test_example_three,
                      TestSuite_CheckLive);
}
