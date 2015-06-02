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
typedef struct _autoresponder_handle_t autoresponder_handle_t;
typedef bool (*autoresponder_t)(request_t *request, void *data);
typedef void (*destructor_t)(void *data);

mock_server2_t *mock_server2_new ();

mock_server2_t *mock_server2_with_autoismaster (int32_t max_wire_version);

int mock_server2_autoresponds (mock_server2_t  *server,
                               autoresponder_t responder,
                               void           *data,
                               destructor_t    destructor);

void mock_server2_remove_autoresponder (mock_server2_t *server,
                                        int id);

int mock_server2_auto_ismaster (mock_server2_t *server,
                                const char     *response_json);


#ifdef MONGOC_ENABLE_SSL
void mock_server2_set_ssl_opts (mock_server2_t    *server,
                                mongoc_ssl_opt_t  *opts);
#endif

uint16_t mock_server2_run (mock_server2_t *server);

const mongoc_uri_t *mock_server2_get_uri (mock_server2_t *server);

bool mock_server2_get_verbose (mock_server2_t *server);

void mock_server2_set_verbose (mock_server2_t *server, bool verbose);

request_t *mock_server2_receives_command (mock_server2_t       *server,
                                          const char           *database_name,
                                          mongoc_query_flags_t  flags,
                                          const char           *command_json);

request_t * mock_server2_receives_query (mock_server2_t          *server,
                                         const char              *ns,
                                         mongoc_query_flags_t     flags,
                                         uint32_t                 skip,
                                         uint32_t                 n_return,
                                         const char              *query_json,
                                         const char              *fields_json);

void mock_server2_hangs_up (request_t *request);

void mock_server2_replies (request_t    *request,
                           uint32_t      flags,
                           int64_t       cursor_id,
                           int32_t       starting_from,
                           int32_t       number_returned,
                           const char   *docs_json);

void mock_server2_destroy (mock_server2_t *server);

void request_destroy (request_t *request);

#endif //MOCK_SERVER2_H
