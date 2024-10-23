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

// `migrating.c` shows examples of migrating from deprecated API to alternatives.

#define FAIL(...)                                                       \
   if (1) {                                                             \
      fprintf (stderr, "[%s:%d] failed: ", __FILE__, (int) (__LINE__)); \
      fprintf (stderr, __VA_ARGS__);                                    \
      fprintf (stderr, "\n");                                           \
      abort ();                                                         \
   } else                                                               \
      (void) 0


#define EXPECT(Cond)                           \
   if (1) {                                    \
      if (!(Cond)) {                           \
         FAIL ("condition failed: %s", #Cond); \
      }                                        \
   } else                                      \
      (void) 0

int
main (int argc, char *argv[])
{
   mongoc_init ();

   mongoc_client_t *client = mongoc_client_new ("mongodb://localhost:27017");
   mongoc_database_t *db = mongoc_client_get_database (client, "db");
   mongoc_collection_t *coll = mongoc_database_get_collection (db, "coll");

   {
      // mongoc_client_command ... before ... begin
      const bson_t *reply;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      mongoc_cursor_t *cursor = mongoc_client_command (client,
                                                       "db",
                                                       MONGOC_QUERY_NONE /* unused */,
                                                       0 /* unused */,
                                                       0 /* unused */,
                                                       0 /* unused */,
                                                       cmd,
                                                       NULL /* unused */,
                                                       NULL /* read prefs */);
      // Expect cursor to return exactly one document for the command reply.
      EXPECT (mongoc_cursor_next (cursor, &reply));

      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      // Expect cursor to return no other documents.
      EXPECT (!mongoc_cursor_next (cursor, &reply));
      mongoc_cursor_destroy (cursor);
      bson_destroy (cmd);
      // mongoc_client_command ... before ... end
   }

   {
      // mongoc_client_command ... after ... begin
      bson_t reply;
      bson_error_t error;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      bool ok = mongoc_client_command_simple (client, "db", cmd, NULL /* read prefs */, &reply, &error);
      if (!ok) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, &reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      bson_destroy (&reply);
      bson_destroy (cmd);
      // mongoc_client_command ... after ... end
   }

   {
      // mongoc_database_command ... before ... begin
      const bson_t *reply;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      mongoc_cursor_t *cursor = mongoc_database_command (db,
                                                         MONGOC_QUERY_NONE /* unused */,
                                                         0 /* unused */,
                                                         0 /* unused */,
                                                         0 /* unused */,
                                                         cmd,
                                                         NULL /* unused */,
                                                         NULL /* read prefs */);
      // Expect cursor to return exactly one document for the command reply.
      EXPECT (mongoc_cursor_next (cursor, &reply));

      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      // Expect cursor to return no other documents.
      EXPECT (!mongoc_cursor_next (cursor, &reply));
      mongoc_cursor_destroy (cursor);
      bson_destroy (cmd);
      // mongoc_database_command ... before ... end
   }

   {
      // mongoc_database_command ... after ... begin
      bson_t reply;
      bson_error_t error;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      bool ok = mongoc_database_command_simple (db, cmd, NULL /* read prefs */, &reply, &error);
      if (!ok) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, &reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      bson_destroy (&reply);
      bson_destroy (cmd);
      // mongoc_database_command ... after ... end
   }

   {
      // mongoc_collection_command ... before ... begin
      const bson_t *reply;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      mongoc_cursor_t *cursor = mongoc_collection_command (coll,
                                                           MONGOC_QUERY_NONE /* unused */,
                                                           0 /* unused */,
                                                           0 /* unused */,
                                                           0 /* unused */,
                                                           cmd,
                                                           NULL /* unused */,
                                                           NULL /* read prefs */);
      // Expect cursor to return exactly one document for the command reply.
      EXPECT (mongoc_cursor_next (cursor, &reply));

      bson_error_t error;
      if (mongoc_cursor_error (cursor, &error)) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      // Expect cursor to return no other documents.
      EXPECT (!mongoc_cursor_next (cursor, &reply));
      mongoc_cursor_destroy (cursor);
      bson_destroy (cmd);
      // mongoc_collection_command ... before ... end
   }

   {
      // mongoc_collection_command ... after ... begin
      bson_t reply;
      bson_error_t error;
      bson_t *cmd = BCON_NEW ("find", "foo", "filter", "{", "}");
      bool ok = mongoc_collection_command_simple (coll, cmd, NULL /* read prefs */, &reply, &error);
      if (!ok) {
         FAIL ("Expected no error, got: %s\n", error.message);
      }

      // Expect successful reply to contain "ok": 1
      bson_iter_t iter;
      EXPECT (bson_iter_init_find (&iter, &reply, "ok") && bson_iter_as_int64 (&iter) == 1);

      bson_destroy (&reply);
      bson_destroy (cmd);
      // mongoc_collection_command ... after ... end
   }

   mongoc_collection_destroy (coll);
   mongoc_database_destroy (db);
   mongoc_client_destroy (client);

   mongoc_cleanup ();
}
