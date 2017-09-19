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

#include <bson.h>
#include "mongoc-change-stream-private.h"
#include "mongoc-error.h"
#include "mongoc-cursor-private.h"

#define CHANGE_STREAM_ERR(_str)         \
   bson_set_error (&stream->err,        \
                   MONGOC_ERROR_CURSOR, \
                   MONGOC_ERROR_BSON,   \
                   "Could not set " _str);


#define SET_BSON_OR_ERR(_dst, _str)                                   \
   do {                                                               \
      if (!BSON_APPEND_VALUE (_dst, _str, bson_iter_value (&iter))) { \
         CHANGE_STREAM_ERR (_str);                                    \
      }                                                               \
   } while (0);

static void
_mongoc_change_stream_make_cursor (mongoc_change_stream_t *stream)
{
   bson_t change_stream_stage; /* { $changeStream: <change_stream_doc> } */
   bson_t change_stream_doc;
   bson_t pipeline;       /* { pipeline: <pipeline_array> */
   bson_t pipeline_array; /* { 0: {...}, 1: {...}, ... } */
   bson_iter_t iter;

   BSON_ASSERT (stream);

   /* Construct the pipeline here, since we need to reconstruct during
    * a retry to add the updated resumeAfter token.
    */
   bson_copy_to (&stream->change_stream_stage_opts, &change_stream_doc);
   bson_init (&change_stream_stage);
   bson_init (&pipeline);

   if (!bson_empty (&stream->resume_token)) {
      bson_concat (&change_stream_doc, &stream->resume_token);
   }

   BSON_APPEND_DOCUMENT (
      &change_stream_stage, "$changeStream", &change_stream_doc);
   bson_destroy (&change_stream_doc);

   bson_append_array_begin (&pipeline, "pipeline", -1, &pipeline_array);
   BSON_APPEND_DOCUMENT (&pipeline_array, "0", &change_stream_stage);
   bson_destroy (&change_stream_stage);

   /* Append user pipeline if it exists */
   if (bson_iter_init_find (&iter, &stream->pipeline_to_append, "pipeline") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      bson_iter_t child_iter;
      uint32_t key_int = 1;
      char buf[16];
      const char *key_str;

      bson_iter_recurse (&iter, &child_iter);
      while (bson_iter_next (&child_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&child_iter)) {
            size_t keyLen =
               bson_uint32_to_string (key_int, &key_str, buf, sizeof (buf));
            bson_append_value (
               &pipeline_array, key_str, keyLen, bson_iter_value (&child_iter));
            ++key_int;
         }
      }
   }

   bson_append_array_end (&pipeline, &pipeline_array);
   bson_destroy (&pipeline_array);

   stream->cursor = mongoc_collection_aggregate (stream->coll,
                                                 MONGOC_QUERY_TAILABLE_CURSOR |
                                                    MONGOC_QUERY_AWAIT_DATA,
                                                 &pipeline,
                                                 &stream->agg_opts,
                                                 NULL);

   bson_destroy (&pipeline);

   if (stream->max_await_time_ms > 0) {
      _mongoc_cursor_set_opt_int64 (stream->cursor,
                                    MONGOC_CURSOR_MAX_AWAIT_TIME_MS,
                                    stream->max_await_time_ms);
   }

   if (stream->batch_size > 0) {
      mongoc_cursor_set_batch_size (stream->cursor, stream->batch_size);
   }
}

mongoc_change_stream_t *
_mongoc_change_stream_new (const mongoc_collection_t *coll,
                           const bson_t *pipeline,
                           const bson_t *opts)
{
   bool full_doc_set = false;
   mongoc_change_stream_t *stream =
      (mongoc_change_stream_t *) bson_malloc (sizeof (mongoc_change_stream_t));

   BSON_ASSERT (coll);
   BSON_ASSERT (pipeline);

   stream->max_await_time_ms = -1;
   stream->batch_size = -1;
   stream->coll = mongoc_collection_copy ((mongoc_collection_t*) coll);
   bson_init (&stream->pipeline_to_append);
   bson_init (&stream->change_stream_stage_opts);
   bson_init (&stream->agg_opts);
   bson_init (&stream->resume_token);
   bson_init (&stream->err_doc);
   memset (&stream->err, 0, sizeof (bson_error_t));
   stream->cursor = NULL;

   /*
    * The passed options may consist of:
    * fullDocument: 'default'|'updateLookup', passed to $changeStream stage
    * resumeAfter: optional<Doc>, passed to $changeStream stage
    * maxAwaitTimeMS: Optional<Int64>, set on the cursor
    * batchSize: Optional<Int32>, passed as agg option, {cursor: { batchSize: }}
    * collation: Optional<Document>, passed as agg option
    */

   if (opts) {
      bson_iter_t iter;

      if (bson_iter_init_find (&iter, opts, "fullDocument")) {
         SET_BSON_OR_ERR (&stream->change_stream_stage_opts, "fullDocument");
         full_doc_set = true;
      }

      if (bson_iter_init_find (&iter, opts, "resumeAfter")) {
         SET_BSON_OR_ERR (&stream->resume_token, "resumeAfter");
      }

      if (bson_iter_init_find (&iter, opts, "batchSize")) {
         SET_BSON_OR_ERR (&stream->agg_opts, "batchSize");
         if (BSON_ITER_HOLDS_INT32 (&iter)) {
            stream->batch_size = bson_iter_int32 (&iter);
         }
      }

      if (bson_iter_init_find (&iter, opts, "collation")) {
         SET_BSON_OR_ERR (&stream->agg_opts, "collation");
      }

      if (bson_iter_init_find (&iter, opts, "maxAwaitTimeMS") &&
          BSON_ITER_HOLDS_INT (&iter)) {
         stream->max_await_time_ms = bson_iter_as_int64 (&iter);
      }
   }

   if (!full_doc_set) {
      if (!BSON_APPEND_UTF8 (
         &stream->change_stream_stage_opts, "fullDocument", "default")) {
         CHANGE_STREAM_ERR ("fullDocument");
      }
   }

   if (!bson_empty (pipeline)) {
      bson_iter_t iter;
      if (bson_iter_init_find (&iter, pipeline, "pipeline")) {
         SET_BSON_OR_ERR (&stream->pipeline_to_append, "pipeline");
      }
   }

   if (stream->err.code == 0) {
      _mongoc_change_stream_make_cursor (stream);
   }

   return stream;
}

bool
mongoc_change_stream_next (mongoc_change_stream_t *stream, const bson_t **bson)
{
   bson_iter_t iter;

   BSON_ASSERT (stream);
   BSON_ASSERT (bson);

   if (stream->err.code != 0) {
      return false;
   }

   if (!mongoc_cursor_next (stream->cursor, bson)) {
      const bson_t *err_doc;
      bson_error_t err;
      bool resumable = false;

      if (!mongoc_cursor_error_document (stream->cursor, &err, &err_doc)) {
         /* No error occurred, just no documents left */
         return false;
      }

      /* Change Streams Spec: An error is resumable if it is not a server error,
       * or if it has error code 43 (cursor not found) or is "not master" */
      if (!bson_empty (err_doc)) {
         /* This is a server error */
         bson_iter_t err_iter;
         if (bson_iter_init_find (&err_iter, err_doc, "errmsg") &&
             BSON_ITER_HOLDS_UTF8 (&err_iter)) {
            uint32_t len;
            const char *errmsg = bson_iter_utf8 (&err_iter, &len);
            if (strncmp (errmsg, "not master", len) == 0) {
               resumable = true;
            }
         }

         if (bson_iter_init_find (&err_iter, err_doc, "code") &&
             BSON_ITER_HOLDS_INT (&err_iter)) {
            if (bson_iter_as_int64 (&err_iter) == 43) {
               resumable = true;
            }
         }
      } else {
         /* This is a client error */
         resumable = true;
      }

      if (resumable) {
         mongoc_cursor_destroy (stream->cursor);
         _mongoc_change_stream_make_cursor (stream);
         if (!mongoc_cursor_next (stream->cursor, bson)) {
            resumable =
               !mongoc_cursor_error_document (stream->cursor, &err, &err_doc);
            if (resumable) {
               /* Empty batch. */
               return false;
            }
         }
      }

      if (!resumable) {
         stream->err = err;
         bson_destroy (&stream->err_doc);
         bson_copy_to (err_doc, &stream->err_doc);
         return false;
      }
   }

   /* We have received documents, either from the first call to next
    * or after a resume. */
   if (!bson_iter_init_find (&iter, *bson, "_id")) {
      bson_set_error (&stream->err,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CHANGE_STREAM_NO_RESUME_TOKEN,
                      "Cannot provide resume functionality when the resume "
                      "token is missing");
      return false;
   }

   /* Copy the resume token */
   bson_reinit (&stream->resume_token);
   BSON_APPEND_VALUE (
      &stream->resume_token, "resumeAfter", bson_iter_value (&iter));
   return true;
}

bool
mongoc_change_stream_error_document (const mongoc_change_stream_t *stream,
                                     bson_error_t *err,
                                     const bson_t **bson)
{
   BSON_ASSERT (stream);

   if (stream->err.code != 0) {
      if (err) {
         *err = stream->err;
      }
      if (bson) {
         *bson = &stream->err_doc;
      }
      return true;
   }
   return false;
}

void
mongoc_change_stream_destroy (mongoc_change_stream_t *stream)
{
   BSON_ASSERT (stream);
   bson_destroy (&stream->pipeline_to_append);
   bson_destroy (&stream->change_stream_stage_opts);
   bson_destroy (&stream->agg_opts);
   bson_destroy (&stream->resume_token);
   bson_destroy (&stream->err_doc);
   if (stream->cursor) {
      mongoc_cursor_destroy (stream->cursor);
   }
   mongoc_collection_destroy (stream->coll);
   bson_free (stream);
}
