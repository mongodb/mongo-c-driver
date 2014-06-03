/*
 * Copyright 2013 MongoDB, Inc.
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
#include "mongoc-collection-private.h"
#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-database.h"
#include "mongoc-database-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-trace.h"
#include "mongoc-util-private.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "database"


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_database_new --
 *
 *       Create a new instance of mongoc_database_t for @client.
 *
 *       @client must stay valid for the life of the resulting
 *       database structure.
 *
 * Returns:
 *       A newly allocated mongoc_database_t that should be freed with
 *       mongoc_database_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_database_t *
_mongoc_database_new (mongoc_client_t              *client,
                      const char                   *name,
                      const mongoc_read_prefs_t    *read_prefs,
                      const mongoc_write_concern_t *write_concern)
{
   mongoc_database_t *db;

   ENTRY;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   db = bson_malloc0(sizeof *db);
   db->client = client;
   db->write_concern = write_concern ?
      mongoc_write_concern_copy(write_concern) :
      mongoc_write_concern_new();
   db->read_prefs = read_prefs ?
      mongoc_read_prefs_copy(read_prefs) :
      mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   bson_strncpy (db->name, name, sizeof db->name);

   RETURN(db);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_destroy --
 *
 *       Releases resources associated with @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Everything.
 *
 *--------------------------------------------------------------------------
 */

void
mongoc_database_destroy (mongoc_database_t *database)
{
   ENTRY;

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

   EXIT;
}


mongoc_cursor_t *
mongoc_database_command (mongoc_database_t         *database,
                         mongoc_query_flags_t       flags,
                         uint32_t              skip,
                         uint32_t              limit,
                         uint32_t              batch_size,
                         const bson_t              *command,
                         const bson_t              *fields,
                         const mongoc_read_prefs_t *read_prefs)
{
   BSON_ASSERT (database);
   BSON_ASSERT (command);

   if (!read_prefs) {
      read_prefs = database->read_prefs;
   }

   return mongoc_client_command (database->client, database->name, flags, skip,
                                 limit, batch_size, command, fields, read_prefs);
}


bool
mongoc_database_command_simple (mongoc_database_t         *database,
                                const bson_t              *command,
                                const mongoc_read_prefs_t *read_prefs,
                                bson_t                    *reply,
                                bson_error_t              *error)
{
   BSON_ASSERT (database);
   BSON_ASSERT (command);

   if (!read_prefs) {
      read_prefs = database->read_prefs;
   }

   return mongoc_client_command_simple (database->client, database->name,
                                        command, read_prefs, reply, error);
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_drop --
 *
 *       Requests that the MongoDB server drops @database, including all
 *       collections and indexes associated with @database.
 *
 *       Make sure this is really what you want!
 *
 * Returns:
 *       true if @database was dropped.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_database_drop (mongoc_database_t *database,
                      bson_error_t      *error)
{
   bool ret;
   bson_t cmd;

   bson_return_val_if_fail(database, false);

   bson_init(&cmd);
   bson_append_int32(&cmd, "dropDatabase", 12, 1);
   ret = mongoc_database_command_simple(database, &cmd, NULL, NULL, error);
   bson_destroy(&cmd);

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_add_user_legacy --
 *
 *       A helper to add a user or update their password on @database.
 *       This uses the legacy protocol by inserting into system.users.
 *
 * Returns:
 *       true if successful; otherwise false and @error is set.
 *
 * Side effects:
 *       @error may be set.
 *
 *--------------------------------------------------------------------------
 */

static bool
mongoc_database_add_user_legacy (mongoc_database_t *database,
                                 const char        *username,
                                 const char        *password,
                                 bson_error_t      *error)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   bool ret = false;
   bson_t query;
   bson_t user;
   char *input;
   char *pwd = NULL;

   ENTRY;

   bson_return_val_if_fail(database, false);
   bson_return_val_if_fail(username, false);
   bson_return_val_if_fail(password, false);

   /*
    * Users are stored in the <dbname>.system.users virtual collection.
    */
   collection = mongoc_client_get_collection(database->client,
                                             database->name,
                                             "system.users");
   BSON_ASSERT(collection);

   /*
    * Hash the users password.
    */
   input = bson_strdup_printf("%s:mongo:%s", username, password);
   pwd = _mongoc_hex_md5(input);
   bson_free(input);

   /*
    * Check to see if the user exists. If so, we will update the
    * password instead of inserting a new user.
    */
   bson_init(&query);
   bson_append_utf8(&query, "user", 4, username, -1);
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &query, NULL, NULL);
   if (!mongoc_cursor_next(cursor, &doc)) {
      if (mongoc_cursor_error(cursor, error)) {
         GOTO (failure);
      }
      bson_init(&user);
      bson_append_utf8(&user, "user", 4, username, -1);
      bson_append_bool(&user, "readOnly", 8, false);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   } else {
      bson_copy_to_excluding(doc, &user, "pwd", (char *)NULL);
      bson_append_utf8(&user, "pwd", 3, pwd, -1);
   }

   if (!mongoc_collection_save(collection, &user, NULL, error)) {
      GOTO (failure_with_user);
   }

   ret = true;

failure_with_user:
   bson_destroy(&user);

failure:
   if (cursor) {
      mongoc_cursor_destroy(cursor);
   }
   mongoc_collection_destroy(collection);
   bson_destroy(&query);
   bson_free(pwd);

   RETURN (ret);
}


bool
mongoc_database_remove_user (mongoc_database_t *database,
                             const char        *username,
                             bson_error_t      *error)
{
   mongoc_collection_t *col;
   bson_error_t lerror;
   bson_t cmd;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (database, false);
   bson_return_val_if_fail (username, false);

   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "dropUser", username);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      bson_init (&cmd);
      BSON_APPEND_UTF8 (&cmd, "user", username);

      col = mongoc_client_get_collection (database->client, database->name,
                                          "system.users");
      BSON_ASSERT (col);

      ret = mongoc_collection_remove (col,
                                      MONGOC_REMOVE_SINGLE_REMOVE,
                                      &cmd,
                                      NULL,
                                      error);

      bson_destroy (&cmd);
      mongoc_collection_destroy (col);
   }

   RETURN (ret);
}


bool
mongoc_database_remove_all_users (mongoc_database_t *database,
                                  bson_error_t      *error)
{
   mongoc_collection_t *col;
   bson_error_t lerror;
   bson_t cmd;
   bool ret;

   ENTRY;

   bson_return_val_if_fail (database, false);

   bson_init (&cmd);
   BSON_APPEND_INT32 (&cmd, "dropAllUsersFromDatabase", 1);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      bson_init (&cmd);

      col = mongoc_client_get_collection (database->client, database->name,
                                          "system.users");
      BSON_ASSERT (col);

      ret = mongoc_collection_remove (col, MONGOC_REMOVE_NONE, &cmd, NULL,
                                      error);

      bson_destroy (&cmd);
      mongoc_collection_destroy (col);
   }

   RETURN (ret);
}


/**
 * mongoc_database_add_user:
 * @database: A #mongoc_database_t.
 * @username: A string containing the username.
 * @password: (allow-none): A string containing password, or NULL.
 * @roles: (allow-none): An optional bson_t of roles.
 * @custom_data: (allow-none): An optional bson_t of data to store.
 * @error: (out) (allow-none): A location for a bson_error_t or %NULL.
 *
 * Creates a new user with access to @database.
 *
 * Returns: None.
 * Side effects: None.
 */
bool
mongoc_database_add_user (mongoc_database_t *database,
                          const char        *username,
                          const char        *password,
                          const bson_t      *roles,
                          const bson_t      *custom_data,
                          bson_error_t      *error)
{
   bson_error_t lerror;
   bson_t cmd;
   bson_t ar;
   char *input;
   char *hashed_password;
   bool ret = false;

   ENTRY;

   BSON_ASSERT (database);
   BSON_ASSERT (username);

   /*
    * CDRIVER-232:
    *
    * Perform a (slow and tedious) round trip to mongod to determine if
    * we can safely call createUser. Otherwise, we will fallback and
    * perform legacy insertion into users collection.
    */
   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "usersInfo", username);
   ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, &lerror);
   bson_destroy (&cmd);

   if (!ret && (lerror.code == MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND)) {
      ret = mongoc_database_add_user_legacy (database, username, password, error);
   } else if (ret) {
      input = bson_strdup_printf ("%s:mongo:%s", username, password);
      hashed_password = _mongoc_hex_md5 (input);
      bson_free (input);

      bson_init (&cmd);
      BSON_APPEND_UTF8 (&cmd, "createUser", username);
      BSON_APPEND_UTF8 (&cmd, "pwd", hashed_password);
      BSON_APPEND_BOOL (&cmd, "digestPassword", false);
      if (custom_data) {
         BSON_APPEND_DOCUMENT (&cmd, "customData", custom_data);
      }
      if (roles) {
         BSON_APPEND_ARRAY (&cmd, "roles", roles);
      } else {
         bson_append_array_begin (&cmd, "roles", 5, &ar);
         bson_append_array_end (&cmd, &ar);
      }

      ret = mongoc_database_command_simple (database, &cmd, NULL, NULL, error);

      bson_free (hashed_password);
      bson_destroy (&cmd);
   } else if (error) {
      memcpy (error, &lerror, sizeof *error);
   }

   RETURN (ret);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_get_read_prefs --
 *
 *       Fetch the read preferences for @database.
 *
 * Returns:
 *       A mongoc_read_prefs_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_read_prefs_t *
mongoc_database_get_read_prefs (const mongoc_database_t *database) /* IN */
{
   bson_return_val_if_fail(database, NULL);
   return database->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_set_read_prefs --
 *
 *       Sets the default read preferences for @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

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


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_get_write_concern --
 *
 *       Fetches the write concern for @database.
 *
 * Returns:
 *       A mongoc_write_concern_t that should not be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

const mongoc_write_concern_t *
mongoc_database_get_write_concern (const mongoc_database_t *database)
{
   bson_return_val_if_fail(database, NULL);

   return database->write_concern;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_database_set_write_concern --
 *
 *       Set the default write concern for @database.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

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


/**
 * mongoc_database_has_collection:
 * @database: (in): A #mongoc_database_t.
 * @name: (in): The name of the collection to check for.
 * @error: (out) (allow-none): A location for a #bson_error_t, or %NULL.
 *
 * Checks to see if a collection exists within the database on the MongoDB
 * server.
 *
 * This will return %false if their was an error communicating with the
 * server, or if the collection does not exist.
 *
 * If @error is provided, it will first be zeroed. Upon error, error.domain
 * will be set.
 *
 * Returns: %true if @name exists, otherwise %false. @error may be set.
 */
bool
mongoc_database_has_collection (mongoc_database_t *database,
                                const char        *name,
                                bson_error_t      *error)
{
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_iter_t iter;
   bool ret = false;
   const char *cur_name;
   bson_t q = BSON_INITIALIZER;
   char ns[140];

   ENTRY;

   BSON_ASSERT (database);
   BSON_ASSERT (name);

   if (error) {
      memset (error, 0, sizeof *error);
   }

   bson_snprintf (ns, sizeof ns, "%s.%s", database->name, name);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   collection = mongoc_client_get_collection (database->client,
                                              database->name,
                                              "system.namespaces");
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                    NULL, read_prefs);

   while (!mongoc_cursor_error (cursor, error) &&
          mongoc_cursor_more (cursor)) {
      while (mongoc_cursor_next (cursor, &doc) &&
          bson_iter_init_find (&iter, doc, "name") &&
          BSON_ITER_HOLDS_UTF8 (&iter)) {
         cur_name = bson_iter_utf8(&iter, NULL);
         if (!strcmp(cur_name, ns)) {
            ret = true;
            GOTO(cleanup);
         }
      }
   }

cleanup:
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_read_prefs_destroy (read_prefs);

   RETURN(ret);
}


char **
mongoc_database_get_collection_names (mongoc_database_t *database,
                                      bson_error_t      *error)
{
   mongoc_collection_t *col;
   mongoc_cursor_t *cursor;
   uint32_t len;
   const bson_t *doc;
   bson_iter_t iter;
   const char *name;
   bson_t q = BSON_INITIALIZER;
   char **ret = NULL;
   int i = 0;

   BSON_ASSERT (database);

   col = mongoc_client_get_collection (database->client,
                                       database->name,
                                       "system.namespaces");

   cursor = mongoc_collection_find (col, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                    NULL, NULL);

   len = (int) strlen (database->name) + 1;

   while (mongoc_cursor_more (cursor) &&
          !mongoc_cursor_error (cursor, error)) {
      if (mongoc_cursor_next (cursor, &doc)) {
         if (bson_iter_init_find (&iter, doc, "name") &&
             BSON_ITER_HOLDS_UTF8 (&iter) &&
             (name = bson_iter_utf8 (&iter, NULL)) &&
             !strchr (name, '$') &&
             (0 == strncmp (name, database->name, len - 1))) {
            ret = bson_realloc (ret, sizeof(char*) * (i + 2));
            ret [i] = bson_strdup (bson_iter_utf8 (&iter, NULL) + len);
            ret [++i] = NULL;
         }
      }
   }

   if (!ret && !mongoc_cursor_error (cursor, error)) {
      ret = bson_malloc0 (sizeof (void*));
   }

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (col);

   return ret;
}


mongoc_collection_t *
mongoc_database_create_collection (mongoc_database_t *database,
                                   const char        *name,
                                   const bson_t      *options,
                                   bson_error_t      *error)
{
   mongoc_collection_t *collection = NULL;
   bson_iter_t iter;
   bson_t cmd;
   bool capped = false;

   bson_return_val_if_fail (database, NULL);
   bson_return_val_if_fail (name, NULL);

   if (strchr (name, '$')) {
      bson_set_error (error,
                      MONGOC_ERROR_NAMESPACE,
                      MONGOC_ERROR_NAMESPACE_INVALID,
                      "The namespace \"%s\" is invalid.",
                      name);
      return NULL;
   }

   if (options) {
      if (bson_iter_init_find (&iter, options, "capped")) {
         if (!BSON_ITER_HOLDS_BOOL (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"capped\" must be a boolean.");
            return NULL;
         }
         capped = bson_iter_bool (&iter);
      }

      if (bson_iter_init_find (&iter, options, "autoIndexId") &&
          !BSON_ITER_HOLDS_BOOL (&iter)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "The argument \"autoIndexId\" must be a boolean.");
         return NULL;
      }

      if (bson_iter_init_find (&iter, options, "size")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"size\" must be an integer.");
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The \"size\" parameter requires {\"capped\": true}");
            return NULL;
         }
      }

      if (bson_iter_init_find (&iter, options, "max")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The argument \"max\" must be an integer.");
            return NULL;
         }
         if (!capped) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "The \"size\" parameter requires {\"capped\": true}");
            return NULL;
         }
      }
   }

   bson_init (&cmd);
   BSON_APPEND_UTF8 (&cmd, "create", name);

   if (options) {
      if (!bson_iter_init (&iter, options)) {
         bson_set_error (error,
                         MONGOC_ERROR_COMMAND,
                         MONGOC_ERROR_COMMAND_INVALID_ARG,
                         "The argument \"options\" is corrupt or invalid.");
         bson_destroy (&cmd);
         return NULL;
      }

      while (bson_iter_next (&iter)) {
         if (!bson_append_iter (&cmd, bson_iter_key (&iter), -1, &iter)) {
            bson_set_error (error,
                            MONGOC_ERROR_COMMAND,
                            MONGOC_ERROR_COMMAND_INVALID_ARG,
                            "Failed to append \"options\" to create command.");
            bson_destroy (&cmd);
            return NULL;
         }
      }
   }

   if (mongoc_database_command_simple (database, &cmd, NULL, NULL, error)) {
      collection = _mongoc_collection_new (database->client,
                                           database->name,
                                           name,
                                           database->read_prefs,
                                           database->write_concern);
   }

   bson_destroy (&cmd);

   return collection;
}


mongoc_collection_t *
mongoc_database_get_collection (mongoc_database_t *database,
                                const char        *collection)
{
   bson_return_val_if_fail (database, NULL);
   bson_return_val_if_fail (collection, NULL);

   return mongoc_client_get_collection (database->client, database->name,
                                        collection);
}


const char *
mongoc_database_get_name (mongoc_database_t *database)
{
   bson_return_val_if_fail (database, NULL);

   return database->name;
}
