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
#include "mongoc-event-private.h"


mongoc_database_t *
mongoc_database_new (mongoc_client_t *client,
                     const char      *name)
{
   mongoc_database_t *db;

   bson_return_val_if_fail(client, NULL);
   bson_return_val_if_fail(name, NULL);

   db = bson_malloc0(sizeof *db);
   db->client = client;
   snprintf(db->name, sizeof db->name, "%s", name);
   db->name[sizeof db->name - 1] = '\0';

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
                         const bson_t         *options,
                         bson_error_t         *error)
{
   char ns[140];

   bson_return_val_if_fail(database, NULL);
   bson_return_val_if_fail(command, NULL);

   snprintf(ns, sizeof ns, "%s.$cmd", database->name);
   return mongoc_cursor_new(database->client, ns, flags, skip, n_return, 0,
                            command, fields, options, error);
}
