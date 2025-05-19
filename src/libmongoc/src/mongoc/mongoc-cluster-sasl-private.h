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

#ifndef MONGOC_CLUSTER_SASL_PRIVATE_H
#define MONGOC_CLUSTER_SASL_PRIVATE_H

#include <mongoc/mongoc-config.h>
#include <mongoc/mongoc-cluster-private.h>
#include <bson/bson.h>

#define AUTH_ERROR_AND_FAIL(...)                                                                  \
   do {                                                                                           \
      bson_set_error (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_AUTHENTICATE, __VA_ARGS__); \
      goto fail;                                                                                  \
   } while (0)

bool
_mongoc_cluster_auth_node_sasl (mongoc_cluster_t *cluster,
                                mongoc_stream_t *stream,
                                mongoc_server_description_t *sd,
                                bson_error_t *error);

bool
_mongoc_sasl_run_command (mongoc_cluster_t *cluster,
                          mongoc_stream_t *stream,
                          mongoc_server_description_t *sd,
                          bson_t *command,
                          bson_t *reply,
                          bson_error_t *error);

#endif /* MONGOC_CLUSTER_SASL_PRIVATE_H */
