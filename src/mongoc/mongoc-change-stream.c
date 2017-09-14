/*
 * Copyright 2017-present MongoDB, Inc.
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

#include "mongoc-change-stream.h"


struct _mongoc_change_stream_t {
   int dummy;
};

bool
mongoc_change_stream_next (mongoc_change_stream_t *stream, const bson_t **bson)
{
   return false;
}

bool
mongoc_change_stream_error_document (const mongoc_change_stream_t *stream,
                                     bson_error_t *err,
                                     const bson_t **err_doc)
{
   return false;
}

void
mongoc_change_stream_destroy (mongoc_change_stream_t *stream)
{
   bson_free (stream);
}

mongoc_change_stream_t *
_mongoc_change_stream_new (const mongoc_collection_t *coll,
                           const bson_t *pipeline,
                           const bson_t *opts)
{
   return (mongoc_change_stream_t *) bson_malloc (
      sizeof (mongoc_change_stream_t));
}