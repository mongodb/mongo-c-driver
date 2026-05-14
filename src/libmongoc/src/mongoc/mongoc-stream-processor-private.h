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

#ifndef MONGOC_STREAM_PROCESSOR_PRIVATE_H
#define MONGOC_STREAM_PROCESSOR_PRIVATE_H

#include <mongoc/mongoc-client.h>
#include <mongoc/mongoc-stream-processing-client.h>

struct _mongoc_stream_processors_t {
   /* not owned; back-pointer to the owning client */
   mongoc_stream_processing_client_t *asp_client;
};

struct _mongoc_stream_processor_t {
   /* not owned; back-pointer to the owning client */
   mongoc_stream_processing_client_t *asp_client;
   char *name; /* owned */
};

#endif /* MONGOC_STREAM_PROCESSOR_PRIVATE_H */
