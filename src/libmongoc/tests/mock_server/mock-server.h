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

#ifndef MOCK_SERVER_H
#define MOCK_SERVER_H

#include <bson/bson.h>

#include "mongoc/mongoc-flags-private.h"
#include "mongoc/mongoc-uri.h"

#ifdef MONGOC_ENABLE_SSL
#include "mongoc/mongoc-ssl.h"
#endif

#include "request.h"

typedef struct _mock_server_t mock_server_t;
typedef struct _autoresponder_handle_t autoresponder_handle_t;
typedef struct _hello_callback_t hello_callback_t;

typedef struct _mock_server_bind_opts_t {
   struct sockaddr_in *bind_addr;
   size_t bind_addr_len;
   int family;
   int ipv6_only;
} mock_server_bind_opts_t;

typedef bool (*autoresponder_t) (request_t *request, void *data);

typedef bool (*hello_callback_func_t) (request_t *request,
                                       void *data,
                                       bson_t *hello_response);

typedef void (*destructor_t) (void *data);

mock_server_t *
mock_server_new ();

mock_server_t *
mock_server_with_auto_hello (int32_t max_wire_version);

mock_server_t *
mock_mongos_new (int32_t max_wire_version);

mock_server_t *
mock_server_down (void);

int
mock_server_autoresponds (mock_server_t *server,
                          autoresponder_t responder,
                          void *data,
                          destructor_t destructor);

void
mock_server_remove_autoresponder (mock_server_t *server, int id);

int
mock_server_auto_hello_callback (mock_server_t *server,
                                 hello_callback_func_t callback_func,
                                 void *data,
                                 destructor_t destructor);

int
mock_server_auto_hello (mock_server_t *server, const char *response_json, ...);

int
mock_server_auto_endsessions (mock_server_t *server);

#ifdef MONGOC_ENABLE_SSL

void
mock_server_set_ssl_opts (mock_server_t *server, mongoc_ssl_opt_t *opts);

#endif

void
mock_server_set_bind_opts (mock_server_t *server,
                           mock_server_bind_opts_t *opts);

uint16_t
mock_server_run (mock_server_t *server);

const mongoc_uri_t *
mock_server_get_uri (mock_server_t *server);

const char *
mock_server_get_host_and_port (mock_server_t *server);

uint16_t
mock_server_get_port (mock_server_t *server);

int64_t
mock_server_get_request_timeout_msec (mock_server_t *server);

void
mock_server_set_request_timeout_msec (mock_server_t *server,
                                      int64_t request_timeout_msec);

bool
mock_server_get_rand_delay (mock_server_t *server);

void
mock_server_set_rand_delay (mock_server_t *server, bool rand_delay);

double
mock_server_get_uptime_sec (mock_server_t *server);

request_t *
mock_server_receives_request (mock_server_t *server);

request_t *
mock_server_receives_command (mock_server_t *server,
                              const char *database_name,
                              mongoc_query_flags_t flags,
                              const char *command_json,
                              ...);

request_t *
mock_server_matches_any_hello_with_json (request_t *request,
                                         const char *match_json_op_msg,
                                         const char *match_json_op_query);

request_t *
mock_server_receives_any_hello (mock_server_t *server);

request_t *
mock_server_receives_legacy_hello (mock_server_t *server,
                                   const char *match_json);

request_t *
mock_server_receives_hello (mock_server_t *server);

request_t *
mock_server_receives_hello_op_msg (mock_server_t *server);

request_t *
mock_server_receives_any_hello_with_match (mock_server_t *server,
                                           const char *match_json_op_msg,
                                           const char *match_json_op_query);

request_t *
mock_server_receives_query (mock_server_t *server,
                            const char *ns,
                            mongoc_query_flags_t flags,
                            uint32_t skip,
                            int32_t n_return,
                            const char *query_json,
                            const char *fields_json);

request_t *
mock_server_receives_insert (mock_server_t *server,
                             const char *ns,
                             mongoc_insert_flags_t flags,
                             const char *doc_json);

request_t *
mock_server_receives_bulk_insert (mock_server_t *server,
                                  const char *ns,
                                  mongoc_insert_flags_t flags,
                                  int n);

request_t *
mock_server_receives_update (mock_server_t *server,
                             const char *ns,
                             mongoc_update_flags_t flags,
                             const char *selector_json,
                             const char *update_json);

request_t *
mock_server_receives_delete (mock_server_t *server,
                             const char *ns,
                             mongoc_remove_flags_t flags,
                             const char *selector_json);

request_t *
mock_server_receives_getmore (mock_server_t *server,
                              const char *ns,
                              int32_t n_return,
                              int64_t cursor_id);

request_t *
mock_server_receives_kill_cursors (mock_server_t *server, int64_t cursor_id);

request_t *
_mock_server_receives_msg (mock_server_t *server, uint32_t flags, ...);
#define mock_server_receives_msg(_server, _flags, ...) \
   _mock_server_receives_msg (_server, _flags, __VA_ARGS__, NULL)

request_t *
mock_server_receives_bulk_msg (mock_server_t *server,
                               uint32_t flags,
                               const bson_t *msg_json,
                               const bson_t *doc_json,
                               size_t n_docs);

void
mock_server_hangs_up (request_t *request);

void
mock_server_resets (request_t *request);

void
mock_server_replies (request_t *request,
                     mongoc_reply_flags_t flags,
                     int64_t cursor_id,
                     int32_t starting_from,
                     int32_t number_returned,
                     const char *docs_json);

void
mock_server_replies_simple (request_t *request, const char *docs_json);

void
mock_server_replies_ok_and_destroys (request_t *request);

void
mock_server_replies_to_find (request_t *request,
                             mongoc_query_flags_t flags,
                             int64_t cursor_id,
                             int32_t number_returned,
                             const char *ns,
                             const char *reply_json,
                             bool is_command);

void
mock_server_replies_opmsg (request_t *request,
                           mongoc_op_msg_flags_t flags,
                           const bson_t *doc);

void
mock_server_reply_multi (request_t *request,
                         mongoc_reply_flags_t flags,
                         const bson_t *docs,
                         int n_docs,
                         int64_t cursor_id);

void
mock_server_destroy (mock_server_t *server);

void
rs_response_to_hello (mock_server_t *server,
                      int max_wire_version,
                      bool primary,
                      int has_tags,
                      ...);

#define RS_RESPONSE_TO_HELLO(server, max_wire_version, primary, has_tags, ...) \
   rs_response_to_hello (                                                      \
      server, max_wire_version, primary, has_tags, __VA_ARGS__, NULL)

#endif /* MOCK_SERVER_H */
