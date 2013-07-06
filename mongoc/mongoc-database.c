/*
 * Copyright 2013 10gen Inc.
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


#include "mongoc-client-private.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-database.h"
#include "mongoc-database-private.h"
#include "mongoc-error.h"


mongoc_database_t *
mongoc_database_new (mongoc_client_t *client,
                     const char      *name)
{
   mongoc_database_t *db;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   db = bson_malloc0(sizeof *db);
   db->client = client;
   strncpy(db->name, name, sizeof db->name);
   db->name[sizeof db->name-1] = '\0';

   return db;
}


void
mongoc_database_destroy (mongoc_database_t *database)
{
   bson_return_if_fail(database);
   bson_free(database);
}


mongoc_cursor_t *
mongoc_database_command (mongoc_database_t    *database,
                         mongoc_query_flags_t  flags,
                         bson_uint32_t         skip,
                         bson_uint32_t         n_return,
                         const bson_t         *command,
                         const bson_t         *fields,
                         mongoc_read_prefs_t  *read_prefs)
{
   char ns[140];

   bson_return_val_if_fail(database, NULL);
   bson_return_val_if_fail(command, NULL);

   snprintf(ns, sizeof ns, "%s.$cmd", database->name);
   return mongoc_cursor_new(database->client, ns, flags, skip, n_return, 0,
                            command, fields, read_prefs);
}


bson_bool_t
mongoc_database_command_simple (mongoc_database_t *database,
                                const bson_t      *cmd,
                                bson_error_t      *error)
{
   mongoc_cursor_t *cursor;
   const bson_t *b;
   bson_iter_t iter;
   bson_bool_t ret = FALSE;
   const char *errmsg = "unknown error";
   char ns[140];

   bson_return_val_if_fail(database, FALSE);
   bson_return_val_if_fail(cmd, FALSE);

   snprintf(ns, sizeof ns, "%s.$cmd", database->name);
   ns[sizeof ns-1] = '\0';

   cursor = mongoc_cursor_new(database->client, ns,
                              MONGOC_QUERY_NONE, 0, 1, 0, cmd, NULL, NULL);

   if (mongoc_cursor_next(cursor, &b) &&
       bson_iter_init_find(&iter, b, "ok") &&
       BSON_ITER_HOLDS_DOUBLE(&iter)) {
      if (bson_iter_double(&iter) == 1.0) {
         ret = TRUE;
      } else {
         if (bson_iter_init_find(&iter, b, "errmsg")) {
            errmsg = bson_iter_utf8(&iter, NULL);
         }
      }
   }

   if (!ret) {
      bson_set_error(error,
                     MONGOC_ERROR_QUERY,
                     MONGOC_ERROR_QUERY_FAILURE,
                     "%s", errmsg);
   }

   mongoc_cursor_destroy(cursor);

   return ret;
}


bson_bool_t
mongoc_database_drop (mongoc_database_t *database,
                      bson_error_t      *error)
{
   bson_bool_t ret;
   bson_t cmd;

   bson_return_val_if_fail(database, FALSE);

   bson_init(&cmd);
   bson_append_int32(&cmd, "dropDatabase", 12, 1);
   ret = mongoc_database_command_simple(database, &cmd, error);
   bson_destroy(&cmd);

   return ret;
}
