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
#include "mongoc-collection.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-database.h"
#include "mongoc-database-private.h"
#include "mongoc-error.h"
#include "mongoc-util-private.h"


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

   if (database->read_prefs) {
      mongoc_read_prefs_destroy(database->read_prefs);
      database->read_prefs = NULL;
   }

   if (database->write_concern) {
      mongoc_write_concern_destroy(database->write_concern);
      database->write_concern = NULL;
   }

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

   if (!read_prefs) {
      read_prefs = database->read_prefs;
   }

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
                              MONGOC_QUERY_NONE, 0, 1, 0, cmd, NULL,
                              database->read_prefs);

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


bson_bool_t
mongoc_database_add_user (mongoc_database_t *database,
                          const char        *username,
                          const char        *password,
                          bson_error_t      *error)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   bson_bool_t ret = FALSE;
   bson_t query;
   bson_t user;
   char *input;
   char *pwd = NULL;

   bson_return_val_if_fail(database, FALSE);
   bson_return_val_if_fail(username, FALSE);
   bson_return_val_if_fail(password, FALSE);

   /*
    * Users are stored in the <dbname>.system.users virtual collection.
    * However, this will likely change to a command soon.
    */
   collection = mongoc_client_get_collection(database->client,
                                             database->name,
                                             "system.users");
   BSON_ASSERT(collection);

   /*
    * Hash the users password.
    */
   input = bson_strdup_printf("%s:mongo:%s", username, password);
   pwd = mongoc_hex_md5(input);
   bson_free(input);

   /*
    * Check to see if the user exists. If so, we will update the
    * password instead of inserting a new user.
    */
   bson_init(&query);
   bson_append_utf8(&query, "user", 4, username, -1);
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1,
                                   &query, NULL, NULL);
   if (!mongoc_cursor_next(cursor, &doc)) {
      if (mongoc_cursor_error(cursor, error)) {
         goto failure;
      }
      bson_init(&user);
      bson_append_utf8(&user, "user", 4, username, -1);
      bson_append_bool(&user, "readOnly", 8, FALSE);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   } else {
      bson_copy_to_excluding(doc, &user, "pwd", NULL);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   }

   if (!mongoc_collection_save(collection, &user, NULL, error)) {
      goto failure;
   }

   ret = TRUE;

failure:
   if (cursor) {
      mongoc_cursor_destroy(cursor);
   }
   mongoc_collection_destroy(collection);
   bson_destroy(&query);
   bson_destroy(&user);
   bson_free(pwd);

   return ret;
}


const mongoc_read_prefs_t *
mongoc_database_get_read_prefs (const mongoc_database_t *database)
{
   bson_return_val_if_fail(database, NULL);
   return database->read_prefs;
}


void
mongoc_database_set_read_prefs (mongoc_database_t         *database,
                                const mongoc_read_prefs_t *read_prefs)
{
   bson_return_if_fail(database);

   if (database->read_prefs) {
      mongoc_read_prefs_destroy(database->read_prefs);
      database->read_prefs = NULL;
   }

   if (read_prefs) {
      database->read_prefs = mongoc_read_prefs_copy(read_prefs);
   }
}


const mongoc_write_concern_t *
mongoc_database_get_write_concern (const mongoc_database_t *database)
{
   bson_return_val_if_fail(database, NULL);
   return database->write_concern;
}


void
mongoc_database_set_write_concern (mongoc_database_t            *database,
                                   const mongoc_write_concern_t *write_concern)
{
   bson_return_if_fail(database);

   if (database->write_concern) {
      mongoc_write_concern_destroy(database->write_concern);
      database->write_concern = NULL;
   }

   if (write_concern) {
      database->write_concern = mongoc_write_concern_copy(write_concern);
   }
}
