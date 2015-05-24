/*
 * Copyright 2015 MongoDB, Inc.
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

#ifndef MOCK_SERVER2_H
#define MOCK_SERVER2_H

#include <bson.h>

#include "mongoc-uri.h"

#ifdef MONGOC_ENABLE_SSL
#include "mongoc-ssl.h"
#endif

typedef struct _mock_server2_t mock_server2_t;
typedef struct _request_t request_t;

mock_server2_t *mock_server2_new ();

uint16_t mock_server2_run (mock_server2_t *server);

const mongoc_uri_t *mock_server2_get_uri (mock_server2_t *server);

bool mock_server2_get_verbose (mock_server2_t *server);

void mock_server2_set_verbose (mock_server2_t *server, bool verbose);

request_t *mock_server2_receives_command (mock_server2_t *server,
                                          const char *database_name,
                                          const char *command_name,
                                          const char *command_json);

void mock_server2_replies (request_t *request,
                           uint32_t flags,
                           int64_t cursor_id,
                           int32_t starting_from,
                           int32_t number_returned,
                           const char *docs_json);

void mock_server2_destroy (mock_server2_t *server);

void request_destroy (request_t *request);

#endif //MOCK_SERVER2_H
