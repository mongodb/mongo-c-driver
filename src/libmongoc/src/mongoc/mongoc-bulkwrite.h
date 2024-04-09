/*
 * Copyright 2024-present MongoDB, Inc.
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

#include "mongoc-prelude.h"

#ifndef MONGOC_BULKWRITE_H
#define MONGOC_BULKWRITE_H

#include <mongoc-client.h>
#include <mongoc-write-concern.h>

BSON_BEGIN_DECLS

typedef struct _mongoc_bulkwriteoptions_t mongoc_bulkwriteoptions_t;
BSON_EXPORT (mongoc_bulkwriteoptions_t *)
mongoc_bulkwriteoptions_new (void);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_ordered (mongoc_bulkwriteoptions_t *self, bool ordered);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_bypassdocumentvalidation (mongoc_bulkwriteoptions_t *self, bool bypassdocumentvalidation);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_let (mongoc_bulkwriteoptions_t *self, const bson_t *let);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_writeconcern (mongoc_bulkwriteoptions_t *self, const mongoc_write_concern_t *writeconcern);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_verboseresults (mongoc_bulkwriteoptions_t *self, bool verboseresults);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_comment (mongoc_bulkwriteoptions_t *self, const bson_t *comment);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_session (mongoc_bulkwriteoptions_t *self, mongoc_client_session_t *session);
// `mongoc_bulkwriteoptions_set_extra` appends `extra` to bulkWrite command.
// It is intended to support future server options.
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_extra (mongoc_bulkwriteoptions_t *self, const bson_t *extra);
// `mongoc_bulkwriteoptions_set_serverid` identifies which server to perform the operation. This is intended for use by
// wrapping drivers that select a server before running the operation.
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_serverid (mongoc_bulkwriteoptions_t *self, uint32_t serverid);
BSON_EXPORT (void)
mongoc_bulkwriteoptions_destroy (mongoc_bulkwriteoptions_t *self);

typedef struct _mongoc_bulkwriteresult_t mongoc_bulkwriteresult_t;
BSON_EXPORT (bool)
mongoc_bulkwriteresult_acknowledged (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_insertedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_upsertedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_matchedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_modifiedcount (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (int64_t)
mongoc_bulkwriteresult_deletedcount (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_verboseresults` returns a document with the fields: `insertResults`, `updateResult`,
// `deleteResults`. Returns NULL if verbose results were not requested.
BSON_EXPORT (const bson_t *)
mongoc_bulkwriteresult_verboseresults (const mongoc_bulkwriteresult_t *self);
// `mongoc_bulkwriteresult_get_serverid` identifies which server to performed the operation. This may differ from a
// previously set serverid if a retry occurred. This is intended for use by wrapping drivers that select a server before
// running the operation.
BSON_EXPORT (uint32_t)
mongoc_bulkwriteresult_serverid (const mongoc_bulkwriteresult_t *self);
BSON_EXPORT (void)
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self);

typedef struct _mongoc_bulkwriteexception_t mongoc_bulkwriteexception_t;
// `mongoc_bulkwriteexception_error` sets `error_document` to a document with the fields: `errorLabels`,
// `writeConcernErrors`, `writeErrors`, `errorReplies`.
BSON_EXPORT (void)
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self,
                                 bson_error_t *error,
                                 const bson_t **error_document /* May be NULL */);
BSON_EXPORT (void)
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self);

// `mongoc_bulkwritereturn_t` may outlive `mongoc_bulkwrite_t`.
typedef struct {
   mongoc_bulkwriteresult_t *res;
   mongoc_bulkwriteexception_t *exc; // May be NULL
} mongoc_bulkwritereturn_t;

typedef struct _mongoc_insertoneopts_t mongoc_insertoneopts_t;
BSON_EXPORT (mongoc_insertoneopts_t *)
mongoc_insertoneopts_new (void);
BSON_EXPORT (void)
mongoc_insertoneopts_set_validation (mongoc_insertoneopts_t *self, bson_validate_flags_t vflags);
BSON_EXPORT (void)
mongoc_insertoneopts_destroy (mongoc_insertoneopts_t *self);

typedef struct _mongoc_bulkwrite_t mongoc_bulkwrite_t;
BSON_EXPORT (mongoc_bulkwrite_t *)
mongoc_client_bulkwrite_new (mongoc_client_t *self, mongoc_bulkwriteoptions_t *opts);
BSON_EXPORT (bool)
mongoc_client_bulkwrite_append_insertone (mongoc_bulkwrite_t *self,
                                          const char *ns,
                                          int ns_len,
                                          const bson_t *document,
                                          mongoc_insertoneopts_t *opts /* May be NULL */,
                                          bson_error_t *error);
BSON_EXPORT (mongoc_bulkwritereturn_t)
mongoc_bulkwrite_execute (mongoc_bulkwrite_t *self);
BSON_EXPORT (void)
mongoc_bulkwrite_destroy (mongoc_bulkwrite_t *self);

BSON_END_DECLS

#endif // MONGOC_BULKWRITE_H
