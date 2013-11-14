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


#ifndef MOCK_SERVER_H
#define MOCK_SERVER_H


#include <bson.h>
#include <mongoc.h>

#include "mongoc-rpc-private.h"


BSON_BEGIN_DECLS


typedef struct _mock_server_t      mock_server_t;
typedef struct _mock_client_info_t mock_client_info_t;


typedef void (*mock_server_handler_t) (mock_server_t   *server,
                                       mongoc_stream_t *stream,
                                       mongoc_rpc_t    *rpc,
                                       void            *user_data);


mock_server_t *mock_server_new              (const char            *address,
                                             bson_uint16_t          port,
                                             mock_server_handler_t  handler,
                                             void                  *handler_data);
void           mock_server_set_wire_version (mock_server_t         *server,
                                             bson_int32_t           min_wire_version,
                                             bson_int32_t           max_wire_version);
int            mock_server_run              (mock_server_t         *server);
void           mock_server_run_in_thread    (mock_server_t         *server);
void           mock_server_quit             (mock_server_t         *server,
                                             int                    code);
void           mock_server_destroy          (mock_server_t         *server);


BSON_END_DECLS


#endif /* MOCK_SERVER_H */
