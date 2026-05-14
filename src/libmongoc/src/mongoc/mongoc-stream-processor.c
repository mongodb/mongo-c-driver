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

#include <mongoc/mongoc-stream-processor-private.h>
#include <mongoc/mongoc-stream-processing-client-private.h>
#include <mongoc/mongoc-stream-processor.h>

#include <mongoc/mongoc-client.h>
#include <mongoc/mongoc-error.h>

#include <bson/bson.h>

#include <string.h>

/* All ASP commands are issued against the admin database. */
#define ASP_DB "admin"

/* Convenience: get the underlying mongoc_client_t from a StreamProcessor. */
static mongoc_client_t *
_sp_client (const mongoc_stream_processor_t *sp)
{
   return sp->asp_client->client;
}

static mongoc_client_t *
_sps_client (const mongoc_stream_processors_t *sps)
{
   return sps->asp_client->client;
}

/* ---------------------------------------------------------------------------
 * StreamProcessors operations
 * ---------------------------------------------------------------------------*/

bool
mongoc_stream_processors_create (mongoc_stream_processors_t *sps,
                                 const char *name,
                                 const bson_t *pipeline,
                                 const mongoc_create_stream_processor_opts_t *opts,
                                 bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t options_doc;
   bson_t reply;
   bool ret;

   BSON_ASSERT (sps);
   BSON_ASSERT (name);
   BSON_ASSERT (pipeline);

   BSON_APPEND_UTF8 (&cmd, "createStreamProcessor", name);
   BSON_APPEND_ARRAY (&cmd, "pipeline", pipeline);

   bson_append_document_begin (&cmd, "options", -1, &options_doc);
   if (opts) {
      if (opts->dlq) {
         BSON_APPEND_DOCUMENT (&options_doc, "dlq", opts->dlq);
      }
      if (opts->stream_meta_field_name) {
         BSON_APPEND_UTF8 (&options_doc, "streamMetaFieldName", opts->stream_meta_field_name);
      }
      if (opts->tier) {
         BSON_APPEND_UTF8 (&options_doc, "tier", opts->tier);
      }
      if (opts->failover == 0) {
         BSON_APPEND_BOOL (&options_doc, "failover", false);
      } else if (opts->failover == 1) {
         BSON_APPEND_BOOL (&options_doc, "failover", true);
      }
   }
   bson_append_document_end (&cmd, &options_doc);

   ret = mongoc_client_command_simple (_sps_client (sps), ASP_DB, &cmd, NULL, &reply, error);
   bson_destroy (&reply);
   bson_destroy (&cmd);
   return ret;
}

mongoc_stream_processor_t *
mongoc_stream_processors_get (mongoc_stream_processors_t *sps, const char *name)
{
   mongoc_stream_processor_t *sp;

   BSON_ASSERT (sps);
   BSON_ASSERT (name);

   sp = bson_malloc0 (sizeof *sp);
   sp->asp_client = sps->asp_client;
   sp->name = bson_strdup (name);
   return sp;
}

/* Parses the getStreamProcessor reply into a mongoc_stream_processor_info_t.
 * Unknown fields are silently ignored per spec. Internal server fields
 * (tenantID, tenantId, projectId, processorId) are not surfaced. */
static mongoc_stream_processor_info_t *
_parse_get_stream_processor_reply (const bson_t *reply)
{
   mongoc_stream_processor_info_t *info;
   bson_iter_t iter;

   info = bson_malloc0 (sizeof *info);
   info->last_state_change_ms = -1;
   info->last_modified_at_ms = -1;
   info->error_code = -1;
   info->error_msg = bson_strdup ("");

   if (!bson_iter_init (&iter, reply)) {
      return info;
   }

   while (bson_iter_next (&iter)) {
      const char *key = bson_iter_key (&iter);

      if (strcmp (key, "name") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->name);
         info->name = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "_id") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->id);
         info->id = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "state") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->state);
         info->state = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "pipeline") == 0 && BSON_ITER_HOLDS_ARRAY (&iter)) {
         bson_t pipeline;
         uint32_t len;
         const uint8_t *data;
         bson_iter_array (&iter, &len, &data);
         if (info->pipeline) {
            bson_destroy (info->pipeline);
         }
         info->pipeline = bson_new_from_data (data, len);
      } else if (strcmp (key, "pipelineVersion") == 0 && BSON_ITER_HOLDS_INT32 (&iter)) {
         info->pipeline_version = bson_iter_int32 (&iter);
      } else if (strcmp (key, "tier") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->tier);
         info->tier = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "dlq") == 0 && BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_t dlq;
         uint32_t len;
         const uint8_t *data;
         bson_iter_document (&iter, &len, &data);
         if (info->dlq) {
            bson_destroy (info->dlq);
         }
         info->dlq = bson_new_from_data (data, len);
      } else if (strcmp (key, "streamMetaFieldName") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->stream_meta_field_name);
         info->stream_meta_field_name = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "enableAutoScaling") == 0 && BSON_ITER_HOLDS_BOOL (&iter)) {
         info->enable_auto_scaling = bson_iter_bool (&iter);
      } else if (strcmp (key, "failoverEnabled") == 0 && BSON_ITER_HOLDS_BOOL (&iter)) {
         info->failover_enabled = bson_iter_bool (&iter);
      } else if (strcmp (key, "activeRegion") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->active_region);
         info->active_region = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "workspaceDefaultRegion") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->workspace_default_region);
         info->workspace_default_region = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "lastStateChange") == 0 && BSON_ITER_HOLDS_DATE_TIME (&iter)) {
         info->last_state_change_ms = bson_iter_date_time (&iter);
      } else if (strcmp (key, "lastModifiedAt") == 0 && BSON_ITER_HOLDS_DATE_TIME (&iter)) {
         info->last_modified_at_ms = bson_iter_date_time (&iter);
      } else if (strcmp (key, "modifiedBy") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->modified_by);
         info->modified_by = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "hasStarted") == 0 && BSON_ITER_HOLDS_BOOL (&iter)) {
         info->has_started = bson_iter_bool (&iter);
      } else if (strcmp (key, "errorMsg") == 0 && BSON_ITER_HOLDS_UTF8 (&iter)) {
         bson_free (info->error_msg);
         info->error_msg = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "errorRetryable") == 0 && BSON_ITER_HOLDS_BOOL (&iter)) {
         info->error_retryable = bson_iter_bool (&iter);
      } else if (strcmp (key, "errorCode") == 0 && BSON_ITER_HOLDS_INT32 (&iter)) {
         info->error_code = bson_iter_int32 (&iter);
      }
      /* Internal server fields (tenantID, tenantId, projectId, processorId,
       * ok) and any unknown fields are intentionally ignored. */
   }

   return info;
}

bool
mongoc_stream_processors_get_info (mongoc_stream_processors_t *sps,
                                   const char *name,
                                   mongoc_stream_processor_info_t **info_out,
                                   bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bool ret;

   BSON_ASSERT (sps);
   BSON_ASSERT (name);
   BSON_ASSERT (info_out);

   BSON_APPEND_UTF8 (&cmd, "getStreamProcessor", name);

   /* getStreamProcessor is a retryable read */
   ret = mongoc_client_read_command_with_opts (
      _sps_client (sps), ASP_DB, &cmd, NULL, NULL, &reply, error);

   if (ret) {
      *info_out = _parse_get_stream_processor_reply (&reply);
   }

   bson_destroy (&reply);
   bson_destroy (&cmd);
   return ret;
}

/* ---------------------------------------------------------------------------
 * StreamProcessor operations
 * ---------------------------------------------------------------------------*/

const char *
mongoc_stream_processor_get_name (const mongoc_stream_processor_t *sp)
{
   BSON_ASSERT (sp);
   return sp->name;
}

bool
mongoc_stream_processor_start (mongoc_stream_processor_t *sp,
                               const mongoc_start_stream_processor_opts_t *opts,
                               bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t options_doc;
   bson_t reply;
   bool ret;

   BSON_ASSERT (sp);

   BSON_APPEND_UTF8 (&cmd, "startStreamProcessor", sp->name);

   if (opts && opts->workers > 0) {
      BSON_APPEND_INT32 (&cmd, "workers", opts->workers);
   }

   bson_append_document_begin (&cmd, "options", -1, &options_doc);
   if (opts) {
      if (opts->clear_checkpoints_set) {
         BSON_APPEND_BOOL (&options_doc, "clearCheckpoints", opts->clear_checkpoints);
      }
      if (opts->start_at_operation_time_set) {
         BSON_APPEND_TIMESTAMP (&options_doc,
                                "startAtOperationTime",
                                opts->start_at_operation_time.timestamp,
                                opts->start_at_operation_time.increment);
      }
      /* startAfter is RESERVED per spec — MUST NOT be serialized to the wire */
      if (opts->tier) {
         BSON_APPEND_UTF8 (&options_doc, "tier", opts->tier);
      }
      if (opts->enable_auto_scaling == 0) {
         BSON_APPEND_BOOL (&options_doc, "enableAutoScaling", false);
      } else if (opts->enable_auto_scaling == 1) {
         BSON_APPEND_BOOL (&options_doc, "enableAutoScaling", true);
      }
   }
   bson_append_document_end (&cmd, &options_doc);

   if (opts && opts->failover) {
      bson_t failover_doc;
      bson_append_document_begin (&cmd, "failover", -1, &failover_doc);
      BSON_APPEND_UTF8 (&failover_doc, "region", opts->failover->region);
      if (opts->failover->mode) {
         BSON_APPEND_UTF8 (&failover_doc, "mode", opts->failover->mode);
      }
      if (opts->failover->dry_run == 0) {
         BSON_APPEND_BOOL (&failover_doc, "dryRun", false);
      } else if (opts->failover->dry_run == 1) {
         BSON_APPEND_BOOL (&failover_doc, "dryRun", true);
      }
      bson_append_document_end (&cmd, &failover_doc);
   }

   ret = mongoc_client_command_simple (_sp_client (sp), ASP_DB, &cmd, NULL, &reply, error);
   bson_destroy (&reply);
   bson_destroy (&cmd);
   return ret;
}

bool
mongoc_stream_processor_stop (mongoc_stream_processor_t *sp, bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bool ret;

   BSON_ASSERT (sp);

   BSON_APPEND_UTF8 (&cmd, "stopStreamProcessor", sp->name);
   ret = mongoc_client_command_simple (_sp_client (sp), ASP_DB, &cmd, NULL, &reply, error);
   bson_destroy (&reply);
   bson_destroy (&cmd);
   return ret;
}

bool
mongoc_stream_processor_drop (mongoc_stream_processor_t *sp, bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bool ret;

   BSON_ASSERT (sp);

   BSON_APPEND_UTF8 (&cmd, "dropStreamProcessor", sp->name);
   ret = mongoc_client_command_simple (_sp_client (sp), ASP_DB, &cmd, NULL, &reply, error);
   bson_destroy (&reply);
   bson_destroy (&cmd);
   return ret;
}

bool
mongoc_stream_processor_stats (mongoc_stream_processor_t *sp,
                               const mongoc_get_stream_processor_stats_opts_t *opts,
                               bson_t *reply,
                               bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t options_doc;
   bool ret;

   BSON_ASSERT (sp);
   BSON_ASSERT (reply);

   BSON_APPEND_UTF8 (&cmd, "getStreamProcessorStats", sp->name);

   bson_append_document_begin (&cmd, "options", -1, &options_doc);
   if (opts && opts->verbose != -1) {
      BSON_APPEND_BOOL (&options_doc, "verbose", opts->verbose != 0);
   }
   bson_append_document_end (&cmd, &options_doc);

   /* getStreamProcessorStats is a retryable read */
   ret = mongoc_client_read_command_with_opts (
      _sp_client (sp), ASP_DB, &cmd, NULL, NULL, reply, error);

   bson_destroy (&cmd);
   return ret;
}

bool
mongoc_stream_processor_get_samples (mongoc_stream_processor_t *sp,
                                     const mongoc_get_stream_processor_samples_opts_t *opts,
                                     mongoc_get_stream_processor_samples_result_t **result_out,
                                     bson_error_t *error)
{
   bson_t cmd = BSON_INITIALIZER;
   bson_t reply;
   bson_iter_t iter;
   int64_t cursor_id = 0;
   int64_t returned_cursor_id = 0;
   mongoc_get_stream_processor_samples_result_t *result = NULL;
   bool ret = false;

   BSON_ASSERT (sp);
   BSON_ASSERT (result_out);

   cursor_id = opts ? opts->cursor_id : 0;

   if (cursor_id == 0) {
      /* Phase 1: open a new sample cursor */
      bson_t start_reply;

      BSON_APPEND_UTF8 (&cmd, "startSampleStreamProcessor", sp->name);
      if (opts && opts->limit > 0) {
         BSON_APPEND_INT32 (&cmd, "limit", opts->limit);
      }

      ret = mongoc_client_command_simple (_sp_client (sp), ASP_DB, &cmd, NULL, &start_reply, error);
      bson_destroy (&cmd);

      if (!ret) {
         bson_destroy (&start_reply);
         return false;
      }

      /* Extract cursorId from startSampleStreamProcessor reply */
      if (bson_iter_init_find (&iter, &start_reply, "cursorId") &&
          BSON_ITER_HOLDS_INT64 (&iter)) {
         cursor_id = bson_iter_int64 (&iter);
      }
      bson_destroy (&start_reply);

      /* Phase 2: immediately fetch the first batch */
      bson_init (&cmd);
      BSON_APPEND_UTF8 (&cmd, "getMoreSampleStreamProcessor", sp->name);
      BSON_APPEND_INT64 (&cmd, "cursorId", cursor_id);
      /* batchSize not sent on the initial getMore */
   } else {
      /* Subsequent call: fetch next batch using provided cursorId */
      BSON_APPEND_UTF8 (&cmd, "getMoreSampleStreamProcessor", sp->name);
      BSON_APPEND_INT64 (&cmd, "cursorId", cursor_id);
      if (opts && opts->batch_size > 0) {
         BSON_APPEND_INT32 (&cmd, "batchSize", opts->batch_size);
      }
   }

   ret = mongoc_client_command_simple (_sp_client (sp), ASP_DB, &cmd, NULL, &reply, error);
   bson_destroy (&cmd);

   if (!ret) {
      bson_destroy (&reply);
      return false;
   }

   result = bson_malloc0 (sizeof *result);

   /* Parse cursorId */
   if (bson_iter_init_find (&iter, &reply, "cursorId") &&
       BSON_ITER_HOLDS_INT64 (&iter)) {
      returned_cursor_id = bson_iter_int64 (&iter);
   }
   result->cursor_id = returned_cursor_id;

   /* Parse messages array */
   if (bson_iter_init_find (&iter, &reply, "messages") &&
       BSON_ITER_HOLDS_ARRAY (&iter)) {
      bson_t messages_array;
      bson_iter_t msg_iter;
      uint32_t array_len;
      const uint8_t *array_data;
      uint32_t count = 0;

      bson_iter_array (&iter, &array_len, &array_data);
      bson_init_static (&messages_array, array_data, array_len);

      /* Count documents */
      if (bson_iter_init (&msg_iter, &messages_array)) {
         while (bson_iter_next (&msg_iter)) {
            count++;
         }
      }

      result->document_count = count;
      if (count > 0) {
         result->documents = bson_malloc (count * sizeof (bson_t *));
         count = 0;
         if (bson_iter_init (&msg_iter, &messages_array)) {
            while (bson_iter_next (&msg_iter)) {
               if (BSON_ITER_HOLDS_DOCUMENT (&msg_iter)) {
                  uint32_t doc_len;
                  const uint8_t *doc_data;
                  bson_iter_document (&msg_iter, &doc_len, &doc_data);
                  result->documents[count++] = bson_new_from_data (doc_data, doc_len);
               }
            }
            result->document_count = count;
         }
      }
   }

   bson_destroy (&reply);
   *result_out = result;
   return true;
}

void
mongoc_stream_processor_destroy (mongoc_stream_processor_t *sp)
{
   if (!sp) {
      return;
   }
   bson_free (sp->name);
   bson_free (sp);
}
