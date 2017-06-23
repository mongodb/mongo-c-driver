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


#include "mongoc-session-private.h"
#include "mongoc-database-private.h"
#include "mongoc-collection-private.h"
#include "mongoc-trace-private.h"


mongoc_session_opt_t *
mongoc_session_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_session_opt_t));
}

void
mongoc_session_opts_set_retry_writes (mongoc_session_opt_t *opts,
                                      bool retry_writes)
{
   ENTRY;

   BSON_ASSERT (opts);

   if (retry_writes) {
      opts->flags |= MONGOC_SESSION_RETRY_WRITES;
   } else {
      opts->flags &= ~MONGOC_SESSION_RETRY_WRITES;
   }

   EXIT;
}

bool
mongoc_session_opts_get_retry_writes (const mongoc_session_opt_t *opts)
{
   ENTRY;

   BSON_ASSERT (opts);

   RETURN (!!(opts->flags & MONGOC_SESSION_RETRY_WRITES));
}

void
mongoc_session_opts_set_causally_consistent_reads (
   mongoc_session_opt_t *opts, bool causally_consistent_reads)
{
   ENTRY;

   BSON_ASSERT (opts);

   if (causally_consistent_reads) {
      opts->flags |= MONGOC_SESSION_CAUSALLY_CONSISTENT_READS;
   } else {
      opts->flags &= ~MONGOC_SESSION_CAUSALLY_CONSISTENT_READS;
   }

   EXIT;
}

bool
mongoc_session_opts_get_causally_consistent_reads (
   const mongoc_session_opt_t *opts)
{
   ENTRY;

   BSON_ASSERT (opts);

   RETURN (!!(opts->flags & MONGOC_SESSION_CAUSALLY_CONSISTENT_READS));
}


mongoc_session_opt_t *
mongoc_session_opts_clone (const mongoc_session_opt_t *opts)
{
   mongoc_session_opt_t *clone;

   ENTRY;

   BSON_ASSERT (opts);

   clone = bson_malloc (sizeof (mongoc_session_opt_t));
   memcpy (clone, opts, sizeof (mongoc_session_opt_t));

   RETURN (clone);
}


void
mongoc_session_opts_destroy (mongoc_session_opt_t *opts)
{
   ENTRY;

   BSON_ASSERT (opts);

   bson_free (opts);

   EXIT;
}


mongoc_session_t *
_mongoc_session_new (mongoc_client_t *client,
                     mongoc_session_opt_t *opts,
                     bson_error_t *error)
{
   mongoc_session_t *session;

   ENTRY;

   BSON_ASSERT (client);
   BSON_ASSERT (opts);

   session = bson_malloc0 (sizeof (mongoc_session_t));
   session->client = client;
   session->server_session_id.value_type = BSON_TYPE_EOD;

   memcpy (&session->opts, opts, sizeof *opts);

   RETURN (session);
}


mongoc_client_t *
mongoc_session_get_client (mongoc_session_t *session)
{
   BSON_ASSERT (session);

   return session->client;
}


const mongoc_session_opt_t *
mongoc_session_get_opts (const mongoc_session_t *session)
{
   BSON_ASSERT (session);

   return &session->opts;
}


const bson_value_t *
mongoc_session_get_session_id (const mongoc_session_t *session)
{
   BSON_ASSERT (session);

   if (session->server_session_id.value_type == BSON_TYPE_EOD) {
      return NULL;
   }

   return &session->server_session_id;
}


mongoc_database_t *
mongoc_session_get_database (mongoc_session_t *session, const char *name)
{
   ENTRY;

   BSON_ASSERT (session);

   RETURN (_mongoc_database_new (
      session->client, name, NULL, NULL, NULL, session));
}


mongoc_collection_t *
mongoc_session_get_collection (mongoc_session_t *session,
                               const char *db,
                               const char *collection)
{
   ENTRY;

   BSON_ASSERT (session);

   RETURN (_mongoc_collection_new (
      session->client, db, collection, NULL, NULL, NULL, session));
}


mongoc_gridfs_t *
mongoc_session_get_gridfs (mongoc_session_t *session,
                           const char *db,
                           const char *prefix,
                           bson_error_t *error)
{
   ENTRY;

   BSON_ASSERT (session);

   RETURN (mongoc_client_get_gridfs (session->client, db, prefix, error));
}


void
mongoc_session_destroy (mongoc_session_t *session)
{
   ENTRY;

   BSON_ASSERT (session);

   bson_free (session);

   EXIT;
}
