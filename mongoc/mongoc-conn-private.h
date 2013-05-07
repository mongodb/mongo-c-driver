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


#ifndef MONGOC_CONN_PRIVATE_H
#define MONGOC_CONN_PRIVATE_H


#include <bson.h>

#include "mongoc-event-private.h"


BSON_BEGIN_DECLS


typedef enum
{
   MONGOC_CONN_TCP,
   MONGOC_CONN_UNIX,
   MONGOC_CONN_FD,
} mongoc_conn_type_t;


typedef enum
{
   MONGOC_CONN_STATE_INITIAL,
   MONGOC_CONN_STATE_CONNECTING,
   MONGOC_CONN_STATE_ESTABLISHED,
   MONGOC_CONN_STATE_DISCONNECTING,
   MONGOC_CONN_STATE_DISCONNECTED,
   MONGOC_CONN_STATE_FAILED,
} mongoc_conn_state_t;


typedef struct
{
   mongoc_conn_state_t  state;
   mongoc_conn_type_t   type;
   int                  rdfd;
   int                  wrfd;
   bson_int32_t         ping;
   char                *host;
   bson_uint16_t        port;
   char                *path;
} mongoc_conn_t;


void
mongoc_conn_init_tcp (mongoc_conn_t *conn,
                      const char    *host,
                      bson_uint16_t  port,
                      const bson_t  *options);


void
mongoc_conn_init_fd (mongoc_conn_t *conn,
                     int            fd,
                     const bson_t  *options);


void
mongoc_conn_init_unix (mongoc_conn_t *conn,
                       const char    *path,
                       const bson_t  *options);


bson_bool_t
mongoc_conn_connect (mongoc_conn_t *conn,
                     bson_error_t  *error);


void
mongoc_conn_disconnect (mongoc_conn_t *conn);


bson_bool_t
mongoc_conn_send (mongoc_conn_t  *conn,
                  mongoc_event_t *event,
                  bson_error_t   *error);


bson_bool_t
mongoc_conn_recv (mongoc_conn_t  *conn,
                  mongoc_event_t *event,
                  bson_error_t   *error);


void
mongoc_conn_destroy (mongoc_conn_t *conn);


BSON_END_DECLS


#endif /* MONGOC_CONN_PRIVATE_H */
