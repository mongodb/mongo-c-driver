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

#include <mongoc/mongoc-prelude.h>

#ifndef MONGOC_STREAM_PROCESSING_CLIENT_H
#define MONGOC_STREAM_PROCESSING_CLIENT_H

#include <bson/bson.h>

#include <mongoc/mongoc-macros.h>

BSON_BEGIN_DECLS

/* Forward declarations */
typedef struct _mongoc_stream_processing_client_t mongoc_stream_processing_client_t;
typedef struct _mongoc_stream_processors_t mongoc_stream_processors_t;
typedef struct _mongoc_stream_processor_t mongoc_stream_processor_t;

/*
 * mongoc_stream_processor_info_t --
 *
 * Information about a stream processor returned by getStreamProcessor.
 * All string fields are owned by this struct; callers MUST NOT free them.
 * Call mongoc_stream_processor_info_destroy() when done.
 *
 * Fields marked Optional may be NULL / -1 when absent from the server response.
 */
typedef struct {
   char *id;                       /* processor id */
   char *name;                     /* processor name */
   char *state;                    /* lifecycle state; see spec for known values */
   bson_t *pipeline;               /* aggregation pipeline; caller must not modify */
   int32_t pipeline_version;
   char *tier;                     /* Optional compute tier, e.g. "SP10" */
   bson_t *dlq;                    /* Optional dead-letter-queue configuration */
   char *stream_meta_field_name;   /* Optional */
   bool enable_auto_scaling;
   bool failover_enabled;
   char *active_region;
   char *workspace_default_region;
   int64_t last_state_change_ms;   /* epoch milliseconds; -1 if absent */
   int64_t last_modified_at_ms;    /* epoch milliseconds; -1 if absent */
   char *modified_by;
   bool has_started;
   char *error_msg;                /* empty string when no error */
   bool error_retryable;
   int32_t error_code;             /* -1 if absent */
} mongoc_stream_processor_info_t;

MONGOC_EXPORT (void)
mongoc_stream_processor_info_destroy (mongoc_stream_processor_info_t *info);

/*
 * Options for createStreamProcessor.
 */
typedef struct {
   bson_t *dlq;                    /* Optional dead-letter-queue document */
   char *stream_meta_field_name;   /* Optional */
   char *tier;                     /* Optional compute tier */
   /* -1 = unset (field omitted), 0 = false, 1 = true */
   int failover;
} mongoc_create_stream_processor_opts_t;

MONGOC_EXPORT (mongoc_create_stream_processor_opts_t *)
mongoc_create_stream_processor_opts_new (void);

MONGOC_EXPORT (void)
mongoc_create_stream_processor_opts_destroy (mongoc_create_stream_processor_opts_t *opts);

/*
 * Options for the failover sub-document in startStreamProcessor.
 */
typedef struct {
   char *region;       /* Required when this struct is used */
   char *mode;         /* Optional: "GRACEFUL" (default) or "FORCED" */
   /* -1 = unset, 0 = false, 1 = true */
   int dry_run;
} mongoc_failover_opts_t;

MONGOC_EXPORT (mongoc_failover_opts_t *)
mongoc_failover_opts_new (const char *region);

MONGOC_EXPORT (void)
mongoc_failover_opts_destroy (mongoc_failover_opts_t *opts);

/*
 * Options for startStreamProcessor.
 *
 * NOTE: startAfter is reserved for future use by the spec. It is present in
 * this struct but is NEVER serialized to the wire. Do not set it.
 */
typedef struct {
   /* 0 means unset (field omitted) */
   int32_t workers;
   /* use _set fields to distinguish "not set" from "false" */
   bool clear_checkpoints;
   bool clear_checkpoints_set;
   bson_timestamp_t start_at_operation_time;
   bool start_at_operation_time_set;
   /* startAfter: RESERVED — never sent on the wire per spec */
   bson_t *start_after; /* present for API completeness only */
   char *tier;          /* Optional: "SP2", "SP5", "SP10", "SP30", "SP50" */
   int enable_auto_scaling; /* -1 = unset, 0 = false, 1 = true */
   mongoc_failover_opts_t *failover; /* Optional; not owned by this struct */
} mongoc_start_stream_processor_opts_t;

MONGOC_EXPORT (mongoc_start_stream_processor_opts_t *)
mongoc_start_stream_processor_opts_new (void);

MONGOC_EXPORT (void)
mongoc_start_stream_processor_opts_destroy (mongoc_start_stream_processor_opts_t *opts);

/*
 * Options for getStreamProcessorStats.
 */
typedef struct {
   int verbose; /* -1 = unset, 0 = false, 1 = true */
} mongoc_get_stream_processor_stats_opts_t;

MONGOC_EXPORT (mongoc_get_stream_processor_stats_opts_t *)
mongoc_get_stream_processor_stats_opts_new (void);

MONGOC_EXPORT (void)
mongoc_get_stream_processor_stats_opts_destroy (mongoc_get_stream_processor_stats_opts_t *opts);

/*
 * Options for getStreamProcessorSamples.
 *
 * When cursor_id is 0 or absent (opts == NULL):
 *   Sends startSampleStreamProcessor (opens a new sample cursor).
 * When cursor_id is non-zero:
 *   Sends getMoreSampleStreamProcessor using that cursor.
 *
 * limit only applies to the initial call (cursor_id == 0).
 * batch_size only applies to subsequent calls (cursor_id != 0).
 */
typedef struct {
   int64_t cursor_id;  /* 0 to open a new cursor */
   int32_t limit;      /* max docs on initial call; 0 = unset */
   int32_t batch_size; /* docs per batch on subsequent calls; 0 = unset */
} mongoc_get_stream_processor_samples_opts_t;

MONGOC_EXPORT (mongoc_get_stream_processor_samples_opts_t *)
mongoc_get_stream_processor_samples_opts_new (void);

MONGOC_EXPORT (void)
mongoc_get_stream_processor_samples_opts_destroy (mongoc_get_stream_processor_samples_opts_t *opts);

/*
 * Result from getStreamProcessorSamples.
 *
 * cursor_id == 0 means the cursor is exhausted; callers MUST NOT call
 * getStreamProcessorSamples again with a zero cursor_id expecting more data.
 *
 * documents is an array of document_count bson_t pointers, each owned by
 * this result. Call mongoc_get_stream_processor_samples_result_destroy()
 * when done.
 */
typedef struct {
   int64_t cursor_id;
   bson_t **documents;
   uint32_t document_count;
} mongoc_get_stream_processor_samples_result_t;

MONGOC_EXPORT (void)
mongoc_get_stream_processor_samples_result_destroy (mongoc_get_stream_processor_samples_result_t *result);

/*
 * StreamProcessingClient --
 *
 * A client connected to an Atlas Stream Processing workspace endpoint.
 * The workspace hostname MUST follow the atlas-stream-* pattern.
 * TLS is enforced and cannot be disabled. authSource defaults to admin.
 *
 * Obtain via mongoc_stream_processing_client_new_from_uri().
 * Do NOT use mongoc_client_new() for workspace connections when ASP
 * semantics (TLS enforcement, dedicated API surface) are required.
 *
 * Additional error codes that may be returned by ASP commands:
 *   9   FailedToParse     - invalid pipeline or command document
 *   72  InvalidOptions    - invalid option values
 *   125 CommandFailed     - general command execution failure
 *   1   InternalError     - unexpected server-side error
 * Additional codes may be returned as the server evolves.
 */
MONGOC_EXPORT (mongoc_stream_processing_client_t *)
mongoc_stream_processing_client_new_from_uri (const mongoc_uri_t *uri, bson_error_t *error);

MONGOC_EXPORT (void)
mongoc_stream_processing_client_destroy (mongoc_stream_processing_client_t *client);

/*
 * Returns a StreamProcessors handle for this workspace.
 * The returned handle is valid for the lifetime of the client and MUST NOT
 * be destroyed independently; it is destroyed with the client.
 */
MONGOC_EXPORT (mongoc_stream_processors_t *)
mongoc_stream_processing_client_get_stream_processors (mongoc_stream_processing_client_t *client);

BSON_END_DECLS

#endif /* MONGOC_STREAM_PROCESSING_CLIENT_H */
