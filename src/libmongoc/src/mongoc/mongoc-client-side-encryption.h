/*
 * Copyright 2019-present MongoDB, Inc.
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

#include "mongoc/mongoc-prelude.h"

#ifndef MONGOC_CLIENT_SIDE_ENCRYPTION_H
#define MONGOC_CLIENT_SIDE_ENCRYPTION_H

#include <bson/bson.h>

/* Forward declare */
struct _mongoc_client_t;

BSON_BEGIN_DECLS

typedef struct _mongoc_auto_encryption_opts_t mongoc_auto_encryption_opts_t;

MONGOC_EXPORT (mongoc_auto_encryption_opts_t *)
mongoc_auto_encryption_opts_new (void);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_destroy (mongoc_auto_encryption_opts_t *opts);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_key_vault_client (
   mongoc_auto_encryption_opts_t *opts, struct _mongoc_client_t *client);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_key_vault_namespace (
   mongoc_auto_encryption_opts_t *opts, const char *db, const char *coll);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_kms_providers (
   mongoc_auto_encryption_opts_t *opts, const bson_t *kms_providers);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_schema_map (mongoc_auto_encryption_opts_t *opts,
                                            const bson_t *schema_map);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_bypass_auto_encryption (
   mongoc_auto_encryption_opts_t *opts, bool bypass_auto_encryption);

MONGOC_EXPORT (void)
mongoc_auto_encryption_opts_set_extra (mongoc_auto_encryption_opts_t *opts,
                                       const bson_t *extra);

BSON_END_DECLS

#endif /* MONGOC_CLIENT_SIDE_ENCRYPTION_H */
