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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_COLLECTION_PRIVATE_H
#define MONGOC_COLLECTION_PRIVATE_H

#include <mongoc/mongoc-client.h>

#include <bson/bson.h>

BSON_BEGIN_DECLS


struct _mongoc_collection_t {
   mongoc_client_t *client;
   char *ns;
   uint32_t nslen;
   char *db;
   char *collection;
   uint32_t collectionlen;
   mongoc_read_prefs_t *read_prefs;
   mongoc_read_concern_t *read_concern;
   mongoc_write_concern_t *write_concern;
   bson_t *gle;
};


mongoc_collection_t *
_mongoc_collection_new (mongoc_client_t *client,
                        const char *db,
                        const char *collection,
                        const mongoc_read_prefs_t *read_prefs,
                        const mongoc_read_concern_t *read_concern,
                        const mongoc_write_concern_t *write_concern);

bool
_mongoc_collection_create_index_if_not_exists (mongoc_collection_t *collection,
                                               const bson_t *keys,
                                               const bson_t *opts,
                                               bson_error_t *error);

BSON_END_DECLS


#endif /* MONGOC_COLLECTION_PRIVATE_H */
