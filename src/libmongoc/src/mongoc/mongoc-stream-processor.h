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

#ifndef MONGOC_STREAM_PROCESSOR_H
#define MONGOC_STREAM_PROCESSOR_H

#include <bson/bson.h>

#include <mongoc/mongoc-macros.h>
#include <mongoc/mongoc-stream-processing-client.h>

BSON_BEGIN_DECLS

/*
 * StreamProcessors --
 *
 * A handle for managing stream processors in a workspace.
 * Obtained via mongoc_stream_processing_client_get_stream_processors().
 * Do NOT call mongoc_stream_processors_destroy() — the handle is owned by
 * the StreamProcessingClient and is freed when the client is destroyed.
 */

/*
 * Creates a new stream processor with the given name and pipeline.
 * Sends the createStreamProcessor command to the admin database.
 *
 * pipeline must be a BSON array document, e.g.
 *   bson_t *pipeline = BCON_NEW ("0", "{", "$source", "{", "}", "}");
 *
 * Returns true on success. On failure sets error and returns false.
 */
MONGOC_EXPORT (bool)
mongoc_stream_processors_create (mongoc_stream_processors_t *sps,
                                 const char *name,
                                 const bson_t *pipeline,
                                 const mongoc_create_stream_processor_opts_t *opts,
                                 bson_error_t *error);

/*
 * Returns a handle for a named stream processor. This call does NOT contact
 * the server; it merely constructs a local handle. The processor may or may
 * not exist on the server.
 *
 * The caller is responsible for calling mongoc_stream_processor_destroy()
 * on the returned handle.
 */
MONGOC_EXPORT (mongoc_stream_processor_t *)
mongoc_stream_processors_get (mongoc_stream_processors_t *sps, const char *name);

/*
 * Returns information about an existing stream processor.
 * Sends the getStreamProcessor command (retryable read).
 *
 * On success, sets *info_out to a newly allocated info struct and returns true.
 * The caller MUST call mongoc_stream_processor_info_destroy() on the result.
 * On failure sets error and returns false.
 */
MONGOC_EXPORT (bool)
mongoc_stream_processors_get_info (mongoc_stream_processors_t *sps,
                                   const char *name,
                                   mongoc_stream_processor_info_t **info_out,
                                   bson_error_t *error);

/*
 * StreamProcessor --
 *
 * A handle for a specific named stream processor. Does not imply the
 * processor currently exists on the server.
 */

/* Returns the processor name. The returned string is valid for the lifetime
 * of the handle. */
MONGOC_EXPORT (const char *)
mongoc_stream_processor_get_name (const mongoc_stream_processor_t *sp);

/*
 * Starts the processor. The processor MUST be in STOPPED or FAILED state;
 * starting a STARTED processor returns a server error.
 * Sends the startStreamProcessor command (not retryable).
 */
MONGOC_EXPORT (bool)
mongoc_stream_processor_start (mongoc_stream_processor_t *sp,
                               const mongoc_start_stream_processor_opts_t *opts,
                               bson_error_t *error);

/*
 * Stops the processor. The processor transitions to STOPPED state and may
 * be restarted later.
 * Sends the stopStreamProcessor command (not retryable).
 */
MONGOC_EXPORT (bool)
mongoc_stream_processor_stop (mongoc_stream_processor_t *sp, bson_error_t *error);

/*
 * Permanently deletes the processor. A dropped processor cannot be recovered.
 * Sends the dropStreamProcessor command (not retryable).
 */
MONGOC_EXPORT (bool)
mongoc_stream_processor_drop (mongoc_stream_processor_t *sp, bson_error_t *error);

/*
 * Returns runtime statistics for a running processor.
 * The processor MUST be in the STARTED state; otherwise the server returns
 * an error.
 * Sends the getStreamProcessorStats command (retryable read).
 *
 * On success, reply is populated with the server's stats document.
 * The caller must call bson_destroy(reply) when done.
 */
MONGOC_EXPORT (bool)
mongoc_stream_processor_stats (mongoc_stream_processor_t *sp,
                               const mongoc_get_stream_processor_stats_opts_t *opts,
                               bson_t *reply,
                               bson_error_t *error);

/*
 * Retrieves a batch of sampled documents from a running stream processor.
 *
 * If opts is NULL or opts->cursor_id is 0:
 *   Sends startSampleStreamProcessor to open a new sample cursor, then
 *   immediately sends getMoreSampleStreamProcessor to fetch the first batch.
 *
 * If opts->cursor_id is non-zero:
 *   Sends getMoreSampleStreamProcessor to fetch the next batch.
 *
 * On success, sets *result_out to a newly allocated result and returns true.
 * The caller MUST check result->cursor_id: a value of 0 means the cursor is
 * exhausted and no further calls should be made.
 * Call mongoc_get_stream_processor_samples_result_destroy() when done.
 *
 * This cursor is NOT a standard MongoDB cursor and MUST NOT be used with
 * the standard getMore command.
 */
MONGOC_EXPORT (bool)
mongoc_stream_processor_get_samples (mongoc_stream_processor_t *sp,
                                     const mongoc_get_stream_processor_samples_opts_t *opts,
                                     mongoc_get_stream_processor_samples_result_t **result_out,
                                     bson_error_t *error);

/*
 * Destroys a StreamProcessor handle. Does not affect the processor on the
 * server. Safe to call with NULL.
 */
MONGOC_EXPORT (void)
mongoc_stream_processor_destroy (mongoc_stream_processor_t *sp);

BSON_END_DECLS

#endif /* MONGOC_STREAM_PROCESSOR_H */
