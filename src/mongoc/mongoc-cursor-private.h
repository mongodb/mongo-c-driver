/*
 * Copyright 2013 MongoDB, Inc.
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

#ifndef MONGOC_CURSOR_PRIVATE_H
#define MONGOC_CURSOR_PRIVATE_H

#if !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>

#include "mongoc-client.h"
#include "mongoc-buffer-private.h"
#include "mongoc-rpc-private.h"
#include "mongoc-server-stream-private.h"


BSON_BEGIN_DECLS

typedef struct _mongoc_cursor_interface_t mongoc_cursor_interface_t;


struct _mongoc_cursor_interface_t
{
   mongoc_cursor_t *(*clone)    (const mongoc_cursor_t  *cursor);
   void             (*destroy)  (mongoc_cursor_t        *cursor);
   bool             (*more)     (mongoc_cursor_t        *cursor);
   bool             (*next)     (mongoc_cursor_t        *cursor,
                                 const bson_t          **bson);
   bool             (*error)    (mongoc_cursor_t        *cursor,
                                 bson_error_t           *error);
   void             (*get_host) (mongoc_cursor_t        *cursor,
                                 mongoc_host_list_t     *host);
};

#define ALLOW_PARTIAL_RESULTS "allowPartialResults"
#define ALLOW_PARTIAL_RESULTS_LEN 19
#define AWAIT_DATA "awaitData"
#define AWAIT_DATA_LEN 9
#define BATCH_SIZE "batchSize"
#define BATCH_SIZE_LEN 9
#define COMMENT "comment"
#define COMMENT_LEN 7
#define EXHAUST "exhaust"
#define EXHAUST_LEN 7
#define FILTER "filter"
#define FILTER_LEN 6
#define FIND "find"
#define FIND_LEN 4
#define HINT "hint"
#define HINT_LEN 4
#define LIMIT "limit"
#define LIMIT_LEN 5
#define MAX "max"
#define MAX_LEN 3
#define MAX_AWAIT_TIME_MS "maxAwaitTimeMS"
#define MAX_AWAIT_TIME_MS_LEN 14
#define MAX_SCAN "maxScan"
#define MAX_SCAN_LEN 7
#define MAX_TIME_MS "maxTimeMS"
#define MAX_TIME_MS_LEN 9
#define MIN "min"
#define MIN_LEN 3
#define NO_CURSOR_TIMEOUT "noCursorTimeout"
#define NO_CURSOR_TIMEOUT_LEN 15
#define OPLOG_REPLAY "oplogReplay"
#define OPLOG_REPLAY_LEN 11
#define ORDERBY "orderby"
#define ORDERBY_LEN 7
#define PROJECTION "projection"
#define PROJECTION_LEN 10
#define QUERY "query"
#define QUERY_LEN 5
#define READ_CONCERN "readConcern"
#define READ_CONCERN_LEN 11
#define RETURN_KEY "returnKey"
#define RETURN_KEY_LEN 9
#define SHOW_DISK_LOC "showDiskLoc"
#define SHOW_DISK_LOC_LEN 11
#define SHOW_RECORD_ID "showRecordId"
#define SHOW_RECORD_ID_LEN 12
#define SINGLE_BATCH "singleBatch"
#define SINGLE_BATCH_LEN 11
#define SKIP "skip"
#define SKIP_LEN 4
#define SNAPSHOT "snapshot"
#define SNAPSHOT_LEN 8
#define SORT "sort"
#define SORT_LEN 4
#define TAILABLE "tailable"
#define TAILABLE_LEN 8

struct _mongoc_cursor_t
{
   mongoc_client_t           *client;

   uint32_t                   server_id;
   bool                       slave_ok;

   unsigned                   is_command      : 1;
   unsigned                   sent            : 1;
   unsigned                   done            : 1;
   unsigned                   end_of_event    : 1;
   unsigned                   has_fields      : 1;
   unsigned                   in_exhaust      : 1;

   bson_t                     filter;
   bson_t                     opts;

   mongoc_read_concern_t     *read_concern;
   mongoc_read_prefs_t       *read_prefs;

   mongoc_write_concern_t    *write_concern;

   uint32_t                   count;

   char                       ns [140];
   uint32_t                   nslen;
   uint32_t                   dblen;

   bson_error_t               error;

   /* for OP_QUERY and OP_GETMORE replies*/
   mongoc_rpc_t               rpc;
   mongoc_buffer_t            buffer;
   bson_reader_t             *reader;
   const bson_t              *current;

   mongoc_cursor_interface_t  iface;
   void                      *iface_data;

   int64_t                    operation_id;
};


int32_t                   _mongoc_n_return            (mongoc_cursor_t              *cursor);
void                      _mongoc_set_cursor_ns       (mongoc_cursor_t              *cursor,
                                                       const char                   *ns,
                                                       uint32_t                      nslen);
bool                      _mongoc_cursor_get_opt_bool (const mongoc_cursor_t        *cursor,
                                                       const char                   *option);
mongoc_cursor_t         *_mongoc_cursor_new_with_opts (mongoc_client_t              *client,
                                                       const char                   *db_and_collection,
                                                       bool                          is_command,
                                                       const bson_t                 *filter,
                                                       const bson_t                 *opts,
                                                       const mongoc_read_prefs_t    *read_prefs,
                                                       const mongoc_read_concern_t  *read_concern);
mongoc_cursor_t *
_mongoc_cursor_new (mongoc_client_t              *client,
                                                       const char                   *db_and_collection,
                                                       mongoc_query_flags_t          flags,
                                                       uint32_t                      skip,
                                                       int32_t                       limit,
                                                       uint32_t                      batch_size,
                                                       bool                          is_command,
                                                       const bson_t                 *query,
                                                       const bson_t                 *fields,
                                                       const mongoc_read_prefs_t    *read_prefs,
                                                       const mongoc_read_concern_t  *read_concern);
mongoc_cursor_t         *_mongoc_cursor_clone         (const mongoc_cursor_t        *cursor);
void                     _mongoc_cursor_destroy       (mongoc_cursor_t              *cursor);
bool                     _mongoc_read_from_buffer     (mongoc_cursor_t              *cursor,
                                                       const bson_t                **bson);
bool                     _use_find_command            (const mongoc_cursor_t        *cursor,
                                                       const mongoc_server_stream_t *server_stream);
mongoc_server_stream_t * _mongoc_cursor_fetch_stream  (mongoc_cursor_t              *cursor);
void                     _mongoc_cursor_collection    (const mongoc_cursor_t        *cursor,
                                                       const char                  **collection,
                                                       int                          *collection_len);
bool                     _mongoc_cursor_op_getmore    (mongoc_cursor_t              *cursor,
                                                       mongoc_server_stream_t       *server_stream);
bool                     _mongoc_cursor_run_command   (mongoc_cursor_t              *cursor,
                                                       const bson_t                 *command,
                                                       bson_t                       *reply);
bool                     _mongoc_cursor_more          (mongoc_cursor_t              *cursor);
bool                     _mongoc_cursor_next          (mongoc_cursor_t              *cursor,
                                                       const bson_t                **bson);
bool                     _mongoc_cursor_error         (mongoc_cursor_t              *cursor,
                                                       bson_error_t                 *error);
void                     _mongoc_cursor_get_host      (mongoc_cursor_t              *cursor,
                                                       mongoc_host_list_t           *host);


BSON_END_DECLS


#endif /* MONGOC_CURSOR_PRIVATE_H */
