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


#ifndef MONGOC_CLIENT_PRIVATE_H
#define MONGOC_CLIENT_PRIVATE_H


#include <bson.h>

#include "mongoc-buffer-private.h"
#include "mongoc-client.h"
#include "mongoc-host-list.h"
#include "mongoc-rpc-private.h"
#include "mongoc-stream.h"


BSON_BEGIN_DECLS


mongoc_stream_t *mongoc_client_create_stream (mongoc_client_t          *client,
                                              const mongoc_host_list_t *host,
                                              bson_error_t             *error);
bson_uint32_t    mongoc_client_sendv         (mongoc_client_t          *client,
                                              mongoc_rpc_t             *rpcs,
                                              size_t                    rpcs_len,
                                              bson_uint32_t             hint,
                                              const bson_t             *options,
                                              bson_error_t             *error);
bson_bool_t      mongoc_client_recv          (mongoc_client_t          *client,
                                              mongoc_rpc_t             *rpc,
                                              mongoc_buffer_t          *buffer,
                                              bson_uint32_t             hint,
                                              bson_error_t             *error);
bson_uint32_t    mongoc_client_stamp         (mongoc_client_t          *client,
                                              bson_uint32_t             node);


BSON_END_DECLS


#endif /* MONGOC_CLIENT_PRIVATE_H */
