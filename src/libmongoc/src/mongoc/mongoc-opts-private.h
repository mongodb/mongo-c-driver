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

#ifndef MONGOC_OPTS_H
#define MONGOC_OPTS_H

#include <mongoc/mongoc-bulk-operation-private.h>
#include <mongoc/mongoc-opts-helpers-private.h>

#include <mongoc/mongoc-client-session.h>

#include <bson/bson.h>

/**************************************************
 *
 * Generated by build/generate-opts.py.
 *
 * DO NOT EDIT THIS FILE.
 *
 *************************************************/
/* clang-format off */

typedef struct _mongoc_crud_opts_t {
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_client_session_t *client_session;
   bson_validate_flags_t validate;
   bson_value_t comment;
} mongoc_crud_opts_t;

typedef struct _mongoc_update_opts_t {
   mongoc_crud_opts_t crud;
   bool bypass;
   bson_t collation;
   bson_value_t hint;
   bool upsert;
   bson_t let;
} mongoc_update_opts_t;

typedef struct _mongoc_insert_one_opts_t {
   mongoc_crud_opts_t crud;
   bool bypass;
   bson_t extra;
} mongoc_insert_one_opts_t;

typedef struct _mongoc_insert_many_opts_t {
   mongoc_crud_opts_t crud;
   bool ordered;
   bool bypass;
   bson_t extra;
} mongoc_insert_many_opts_t;

typedef struct _mongoc_delete_opts_t {
   mongoc_crud_opts_t crud;
   bson_t collation;
   bson_value_t hint;
   bson_t let;
} mongoc_delete_opts_t;

typedef struct _mongoc_delete_one_opts_t {
   mongoc_delete_opts_t delete;
   bson_t extra;
} mongoc_delete_one_opts_t;

typedef struct _mongoc_delete_many_opts_t {
   mongoc_delete_opts_t delete;
   bson_t extra;
} mongoc_delete_many_opts_t;

typedef struct _mongoc_update_one_opts_t {
   mongoc_update_opts_t update;
   bson_t arrayFilters;
   bson_t sort;
   bson_t extra;
} mongoc_update_one_opts_t;

typedef struct _mongoc_update_many_opts_t {
   mongoc_update_opts_t update;
   bson_t arrayFilters;
   bson_t extra;
} mongoc_update_many_opts_t;

typedef struct _mongoc_replace_one_opts_t {
   mongoc_update_opts_t update;
   bson_t sort;
   bson_t extra;
} mongoc_replace_one_opts_t;

typedef struct _mongoc_bulk_opts_t {
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   bool ordered;
   mongoc_client_session_t *client_session;
   bson_t let;
   bson_value_t comment;
   bson_t extra;
} mongoc_bulk_opts_t;

typedef struct _mongoc_bulk_insert_opts_t {
   bson_validate_flags_t validate;
   bson_t extra;
} mongoc_bulk_insert_opts_t;

typedef struct _mongoc_bulk_update_opts_t {
   bson_validate_flags_t validate;
   bson_t collation;
   bson_value_t hint;
   bool upsert;
   bool multi;
} mongoc_bulk_update_opts_t;

typedef struct _mongoc_bulk_update_one_opts_t {
   mongoc_bulk_update_opts_t update;
   bson_t sort;
   bson_t arrayFilters;
   bson_t extra;
} mongoc_bulk_update_one_opts_t;

typedef struct _mongoc_bulk_update_many_opts_t {
   mongoc_bulk_update_opts_t update;
   bson_t arrayFilters;
   bson_t extra;
} mongoc_bulk_update_many_opts_t;

typedef struct _mongoc_bulk_replace_one_opts_t {
   mongoc_bulk_update_opts_t update;
   bson_t sort;
   bson_t extra;
} mongoc_bulk_replace_one_opts_t;

typedef struct _mongoc_bulk_remove_opts_t {
   bson_t collation;
   bson_value_t hint;
   int32_t limit;
} mongoc_bulk_remove_opts_t;

typedef struct _mongoc_bulk_remove_one_opts_t {
   mongoc_bulk_remove_opts_t remove;
   bson_t extra;
} mongoc_bulk_remove_one_opts_t;

typedef struct _mongoc_bulk_remove_many_opts_t {
   mongoc_bulk_remove_opts_t remove;
   bson_t extra;
} mongoc_bulk_remove_many_opts_t;

typedef struct _mongoc_change_stream_opts_t {
   int32_t batchSize;
   bson_t resumeAfter;
   bson_t startAfter;
   mongoc_timestamp_t startAtOperationTime;
   int64_t maxAwaitTimeMS;
   const char *fullDocument;
   const char *fullDocumentBeforeChange;
   bool showExpandedEvents;
   bson_value_t comment;
   bson_t extra;
} mongoc_change_stream_opts_t;

typedef struct _mongoc_create_index_opts_t {
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_client_session_t *client_session;
   bson_t extra;
} mongoc_create_index_opts_t;

typedef struct _mongoc_read_write_opts_t {
   bson_t readConcern;
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_client_session_t *client_session;
   bson_t collation;
   uint32_t serverId;
   bson_t extra;
} mongoc_read_write_opts_t;

typedef struct _mongoc_gridfs_bucket_opts_t {
   const char *bucketName;
   int32_t chunkSizeBytes;
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_read_concern_t *readConcern;
   bson_t extra;
} mongoc_gridfs_bucket_opts_t;

typedef struct _mongoc_gridfs_bucket_upload_opts_t {
   int32_t chunkSizeBytes;
   bson_t metadata;
   bson_t extra;
} mongoc_gridfs_bucket_upload_opts_t;

typedef struct _mongoc_aggregate_opts_t {
   mongoc_read_concern_t *readConcern;
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_client_session_t *client_session;
   bool bypass;
   bson_t collation;
   uint32_t serverId;
   int32_t batchSize;
   bool batchSize_is_set;
   bson_t let;
   bson_value_t comment;
   bson_value_t hint;
   bson_t extra;
} mongoc_aggregate_opts_t;

typedef struct _mongoc_find_and_modify_appended_opts_t {
   mongoc_write_concern_t *writeConcern;
   bool write_concern_owned;
   mongoc_client_session_t *client_session;
   bson_value_t hint;
   bson_t let;
   bson_value_t comment;
   bson_t extra;
} mongoc_find_and_modify_appended_opts_t;

typedef struct _mongoc_count_document_opts_t {
   bson_t readConcern;
   mongoc_client_session_t *client_session;
   bson_t collation;
   uint32_t serverId;
   bson_value_t skip;
   bson_value_t limit;
   bson_value_t comment;
   bson_value_t hint;
   bson_t extra;
} mongoc_count_document_opts_t;

bool
_mongoc_insert_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_insert_one_opts_t *mongoc_insert_one_opts,
   bson_error_t *error);

void
_mongoc_insert_one_opts_cleanup (mongoc_insert_one_opts_t *mongoc_insert_one_opts);

bool
_mongoc_insert_many_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_insert_many_opts_t *mongoc_insert_many_opts,
   bson_error_t *error);

void
_mongoc_insert_many_opts_cleanup (mongoc_insert_many_opts_t *mongoc_insert_many_opts);

bool
_mongoc_delete_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_delete_one_opts_t *mongoc_delete_one_opts,
   bson_error_t *error);

void
_mongoc_delete_one_opts_cleanup (mongoc_delete_one_opts_t *mongoc_delete_one_opts);

bool
_mongoc_delete_many_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_delete_many_opts_t *mongoc_delete_many_opts,
   bson_error_t *error);

void
_mongoc_delete_many_opts_cleanup (mongoc_delete_many_opts_t *mongoc_delete_many_opts);

bool
_mongoc_update_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_update_one_opts_t *mongoc_update_one_opts,
   bson_error_t *error);

void
_mongoc_update_one_opts_cleanup (mongoc_update_one_opts_t *mongoc_update_one_opts);

bool
_mongoc_update_many_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_update_many_opts_t *mongoc_update_many_opts,
   bson_error_t *error);

void
_mongoc_update_many_opts_cleanup (mongoc_update_many_opts_t *mongoc_update_many_opts);

bool
_mongoc_replace_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_replace_one_opts_t *mongoc_replace_one_opts,
   bson_error_t *error);

void
_mongoc_replace_one_opts_cleanup (mongoc_replace_one_opts_t *mongoc_replace_one_opts);

bool
_mongoc_bulk_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_opts_t *mongoc_bulk_opts,
   bson_error_t *error);

void
_mongoc_bulk_opts_cleanup (mongoc_bulk_opts_t *mongoc_bulk_opts);

bool
_mongoc_bulk_insert_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_insert_opts_t *mongoc_bulk_insert_opts,
   bson_error_t *error);

void
_mongoc_bulk_insert_opts_cleanup (mongoc_bulk_insert_opts_t *mongoc_bulk_insert_opts);

bool
_mongoc_bulk_update_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_update_one_opts_t *mongoc_bulk_update_one_opts,
   bson_error_t *error);

void
_mongoc_bulk_update_one_opts_cleanup (mongoc_bulk_update_one_opts_t *mongoc_bulk_update_one_opts);

bool
_mongoc_bulk_update_many_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_update_many_opts_t *mongoc_bulk_update_many_opts,
   bson_error_t *error);

void
_mongoc_bulk_update_many_opts_cleanup (mongoc_bulk_update_many_opts_t *mongoc_bulk_update_many_opts);

bool
_mongoc_bulk_replace_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_replace_one_opts_t *mongoc_bulk_replace_one_opts,
   bson_error_t *error);

void
_mongoc_bulk_replace_one_opts_cleanup (mongoc_bulk_replace_one_opts_t *mongoc_bulk_replace_one_opts);

bool
_mongoc_bulk_remove_one_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_remove_one_opts_t *mongoc_bulk_remove_one_opts,
   bson_error_t *error);

void
_mongoc_bulk_remove_one_opts_cleanup (mongoc_bulk_remove_one_opts_t *mongoc_bulk_remove_one_opts);

bool
_mongoc_bulk_remove_many_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_bulk_remove_many_opts_t *mongoc_bulk_remove_many_opts,
   bson_error_t *error);

void
_mongoc_bulk_remove_many_opts_cleanup (mongoc_bulk_remove_many_opts_t *mongoc_bulk_remove_many_opts);

bool
_mongoc_change_stream_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_change_stream_opts_t *mongoc_change_stream_opts,
   bson_error_t *error);

void
_mongoc_change_stream_opts_cleanup (mongoc_change_stream_opts_t *mongoc_change_stream_opts);

bool
_mongoc_create_index_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_create_index_opts_t *mongoc_create_index_opts,
   bson_error_t *error);

void
_mongoc_create_index_opts_cleanup (mongoc_create_index_opts_t *mongoc_create_index_opts);

bool
_mongoc_read_write_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_read_write_opts_t *mongoc_read_write_opts,
   bson_error_t *error);

void
_mongoc_read_write_opts_cleanup (mongoc_read_write_opts_t *mongoc_read_write_opts);

bool
_mongoc_gridfs_bucket_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_gridfs_bucket_opts_t *mongoc_gridfs_bucket_opts,
   bson_error_t *error);

void
_mongoc_gridfs_bucket_opts_cleanup (mongoc_gridfs_bucket_opts_t *mongoc_gridfs_bucket_opts);

bool
_mongoc_gridfs_bucket_upload_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_gridfs_bucket_upload_opts_t *mongoc_gridfs_bucket_upload_opts,
   bson_error_t *error);

void
_mongoc_gridfs_bucket_upload_opts_cleanup (mongoc_gridfs_bucket_upload_opts_t *mongoc_gridfs_bucket_upload_opts);

bool
_mongoc_aggregate_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_aggregate_opts_t *mongoc_aggregate_opts,
   bson_error_t *error);

void
_mongoc_aggregate_opts_cleanup (mongoc_aggregate_opts_t *mongoc_aggregate_opts);

bool
_mongoc_find_and_modify_appended_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_find_and_modify_appended_opts_t *mongoc_find_and_modify_appended_opts,
   bson_error_t *error);

void
_mongoc_find_and_modify_appended_opts_cleanup (mongoc_find_and_modify_appended_opts_t *mongoc_find_and_modify_appended_opts);

bool
_mongoc_count_document_opts_parse (
   mongoc_client_t *client,
   const bson_t *opts,
   mongoc_count_document_opts_t *mongoc_count_document_opts,
   bson_error_t *error);

void
_mongoc_count_document_opts_cleanup (mongoc_count_document_opts_t *mongoc_count_document_opts);

#endif /* MONGOC_OPTS_H */
