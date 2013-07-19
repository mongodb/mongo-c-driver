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


#ifndef MONGOC_CLIENT_H
#define MONGOC_CLIENT_H


#include <bson.h>

#include "mongoc-collection.h"
#include "mongoc-database.h"
#include "mongoc-stream.h"
#include "mongoc-uri.h"
#include "mongoc-write-concern.h"


BSON_BEGIN_DECLS


#define MONGOC_NAMESPACE_MAX 128


/**
 * mongoc_client_t:
 *
 * The mongoc_client_t structure maintains information about a connection to
 * a MongoDB server.
 */
typedef struct _mongoc_client_t mongoc_client_t;


/**
 * mongoc_stream_initiator_t:
 * @uri: The uri and options for the stream.
 * @host: The host and port (or UNIX domain socket path) to connect to.
 * @error: A location for an error.
 *
 * Creates a new mongoc_stream_t for the host and port. This can be used
 * by language bindings to create network transports other than those
 * built into libmongoc. An example of such would be the streams API
 * provided by PHP.
 *
 * Returns: A newly allocated mongoc_stream_t or NULL on failure.
 */
typedef mongoc_stream_t *(*mongoc_stream_initiator_t) (const mongoc_uri_t       *uri,
                                                       const mongoc_host_list_t *host,
                                                       void                     *user_data,
                                                       bson_error_t             *error);


mongoc_client_t     *mongoc_client_new                  (const char *uri_string);
mongoc_client_t     *mongoc_client_new_from_uri         (const mongoc_uri_t *uri);
const mongoc_uri_t  *mongoc_client_get_uri              (const mongoc_client_t *client);
void                 mongoc_client_set_stream_initiator (mongoc_client_t           *client,
                                                         mongoc_stream_initiator_t  initiator,
                                                         void                      *user_data);
void                 mongoc_client_destroy              (mongoc_client_t *client);
mongoc_database_t   *mongoc_client_get_database         (mongoc_client_t *client,
                                                         const char      *name);
mongoc_collection_t *mongoc_client_get_collection       (mongoc_client_t *client,
                                                         const char      *db,
                                                         const char      *collection);

const mongoc_write_concern_t *mongoc_client_get_write_concern (mongoc_client_t              *client);
void                          mongoc_client_set_write_concern (mongoc_client_t              *client,
                                                               const mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_CLIENT_H */
