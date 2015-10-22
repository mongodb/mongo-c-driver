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


#include "mongoc-server-stream-private.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "server-stream"


mongoc_server_stream_t *
mongoc_server_stream_new (mongoc_topology_description_type_t topology_type,
                          mongoc_server_description_t *sd,
                          mongoc_stream_t *stream)
{
   mongoc_server_stream_t *server_stream;

   BSON_ASSERT (sd);
   BSON_ASSERT (stream);

   server_stream = bson_malloc (sizeof (mongoc_server_stream_t));
   server_stream->topology_type = topology_type;
   server_stream->sd = sd;                       /* becomes owned */
   server_stream->stream = stream;               /* merely borrowed */

   return server_stream;
}

void
mongoc_server_stream_cleanup (mongoc_server_stream_t *server_stream)
{
   if (server_stream) {
      mongoc_server_description_destroy (server_stream->sd);
      bson_free (server_stream);
   }
}

