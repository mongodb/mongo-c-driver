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

#ifndef MONGOC_SESSION_H
#define MONGOC_SESSION_H

#if !defined(MONGOC_INSIDE) && !defined(MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>
#include "mongoc-macros.h"
#include "mongoc-read-prefs.h"
#include "mongoc-read-concern.h"
#include "mongoc-write-concern.h"

BSON_BEGIN_DECLS

typedef struct _mongoc_client_t mongoc_client_t;
typedef struct _mongoc_database_t mongoc_database_t;
typedef struct _mongoc_collection_t mongoc_collection_t;
typedef struct _mongoc_gridfs_t mongoc_gridfs_t;
typedef struct _mongoc_session_opt_t mongoc_session_opt_t;
typedef struct _mongoc_session_t mongoc_session_t;

MONGOC_EXPORT (mongoc_session_opt_t *)
mongoc_session_opts_new (void) BSON_GNUC_WARN_UNUSED_RESULT;

MONGOC_EXPORT (void)
mongoc_session_opts_set_retry_writes (mongoc_session_opt_t *opts,
                                      bool retry_writes);

MONGOC_EXPORT (bool)
mongoc_session_opts_get_retry_writes (const mongoc_session_opt_t *opts);

MONGOC_EXPORT (void)
mongoc_session_opts_set_causally_consistent_reads (
   mongoc_session_opt_t *opts, bool causally_consistent_reads);

MONGOC_EXPORT (bool)
mongoc_session_opts_get_causally_consistent_reads (
   const mongoc_session_opt_t *opts);

MONGOC_EXPORT (mongoc_session_opt_t *)
mongoc_session_opts_clone (const mongoc_session_opt_t *opts);

MONGOC_EXPORT (void)
mongoc_session_opts_destroy (mongoc_session_opt_t *opts);

MONGOC_EXPORT (mongoc_client_t *)
mongoc_session_get_client (mongoc_session_t *session);

MONGOC_EXPORT (const mongoc_session_opt_t *)
mongoc_session_get_opts (const mongoc_session_t *session);

MONGOC_EXPORT (const bson_value_t *)
mongoc_session_get_session_id (const mongoc_session_t *session);

MONGOC_EXPORT (mongoc_database_t *)
mongoc_session_get_database (mongoc_session_t *session,
                             const char *name) BSON_GNUC_WARN_UNUSED_RESULT;

MONGOC_EXPORT (mongoc_collection_t *)
mongoc_session_get_collection (mongoc_session_t *session,
                               const char *db,
                               const char *collection)
   BSON_GNUC_WARN_UNUSED_RESULT;

MONGOC_EXPORT (mongoc_gridfs_t *)
mongoc_session_get_gridfs (mongoc_session_t *session,
                           const char *db,
                           const char *prefix,
                           bson_error_t *error) BSON_GNUC_WARN_UNUSED_RESULT;

MONGOC_EXPORT (bool)
mongoc_session_read_command_with_opts (mongoc_session_t *session,
                                       const char *db_name,
                                       const bson_t *command,
                                       const mongoc_read_prefs_t *read_prefs,
                                       const bson_t *opts,
                                       bson_t *reply,
                                       bson_error_t *error);
MONGOC_EXPORT (bool)
mongoc_session_write_command_with_opts (mongoc_session_t *session,
                                        const char *db_name,
                                        const bson_t *command,
                                        const bson_t *opts,
                                        bson_t *reply,
                                        bson_error_t *error);

MONGOC_EXPORT (bool)
mongoc_session_read_write_command_with_opts (
   mongoc_session_t *session,
   const char *db_name,
   const bson_t *command,
   const mongoc_read_prefs_t *read_prefs,
   const bson_t *opts,
   bson_t *reply,
   bson_error_t *error);

MONGOC_EXPORT (const mongoc_write_concern_t *)
mongoc_session_get_write_concern (const mongoc_session_t *session);

MONGOC_EXPORT (const mongoc_read_concern_t *)
mongoc_session_get_read_concern (const mongoc_session_t *session);

MONGOC_EXPORT (const mongoc_read_prefs_t *)
mongoc_session_get_read_prefs (const mongoc_session_t *session);

/* There is no mongoc_session_end, only mongoc_session_destroy. Driver Sessions
 * Spec: "In languages that have idiomatic ways of disposing of resources,
 * drivers SHOULD support that in addition to or instead of endSession."
 */

MONGOC_EXPORT (void)
mongoc_session_destroy (mongoc_session_t *uri);

BSON_END_DECLS


#endif /* MONGOC_SESSION_H */
