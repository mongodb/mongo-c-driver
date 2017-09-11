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
/* mongoc_client_session_t and mongoc_session_opt_t are typedef'ed here */
#include "mongoc-client.h"

BSON_BEGIN_DECLS

MONGOC_EXPORT (mongoc_session_opt_t *)
mongoc_session_opts_new (void) BSON_GNUC_WARN_UNUSED_RESULT;

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
mongoc_client_session_get_client (mongoc_client_session_t *session);

MONGOC_EXPORT (const mongoc_session_opt_t *)
mongoc_client_session_get_opts (const mongoc_client_session_t *session);

MONGOC_EXPORT (const bson_t *)
mongoc_client_session_get_session_id (const mongoc_client_session_t *session);


/* There is no mongoc_client_session_end, only mongoc_client_session_destroy.
 * Driver Sessions Spec: "In languages that have idiomatic ways of disposing of
 * resources, drivers SHOULD support that in addition to or instead of
 * endSession."
 */

MONGOC_EXPORT (void)
mongoc_client_session_destroy (mongoc_client_session_t *uri);

BSON_END_DECLS


#endif /* MONGOC_SESSION_H */
