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

#ifndef MONGOC_STREAM_PROCESSING_CLIENT_PRIVATE_H
#define MONGOC_STREAM_PROCESSING_CLIENT_PRIVATE_H

#include <mongoc/mongoc-client.h>
#include <mongoc/mongoc-stream-processing-client.h>

/* mongoc_stream_processors_t is stored by pointer; forward declaration suffices. */

struct _mongoc_stream_processing_client_t {
   mongoc_client_t *client;             /* owned */
   mongoc_stream_processors_t *sps;     /* owned; returned by get_stream_processors */
};

/* Returns true if the given hostname looks like an Atlas Stream Processing
 * workspace endpoint (starts with "atlas-stream-" or ends with
 * ".a.query.mongodb.net"). */
bool
_mongoc_is_asp_workspace_host (const char *host);

#endif /* MONGOC_STREAM_PROCESSING_CLIENT_PRIVATE_H */
