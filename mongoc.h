/*
 * Copyright 2013 10gen Inc.
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


#ifndef MONGOC_H
#define MONGOC_H


#include <bson.h>

#include "mongoc-uri.h"


BSON_BEGIN_DECLS


/**
 * mongoc_client_t:
 *
 * The mongoc_client_t structure maintains information about a connection to
 * a MongoDB server.
 */
typedef struct _mongoc_client_t mongoc_client_t;


/**
 * mongoc_delete_flags_t:
 * @MONGOC_DELETE_NONE: Specify no delete flags.
 * @MONGOC_DELETE_SINGLE_REMOVE: Only remove the first document matching the
 *    document selector.
 *
 * #mongoc_delete_flags_t are used when performing a delete operation.
 */
typedef enum
{
   MONGOC_DELETE_NONE          = 0,
   MONGOC_DELETE_SINGLE_REMOVE = 1 << 0,
} mongoc_delete_flags_t;


/**
 * mongoc_insert_flags_t:
 * @MONGOC_INSERT_NONE: Specify no insert flags.
 * @MONGOC_INSERT_CONTINUE_ON_ERROR: Continue inserting documents from
 *    the insertion set even if one fails.
 *
 * #mongoc_insert_flags_t are used when performing an insert operation.
 */
typedef enum
{
   MONGOC_INSERT_NONE              = 0,
   MONGOC_INSERT_CONTINUE_ON_ERROR = 1 << 0,
} mongoc_insert_flags_t;


/**
 * mongoc_query_flags_t:
 * @MONGOC_QUERY_NONE: No query flags supplied.
 * @MONGOC_QUERY_TAILABLE_CURSOR: Cursor will not be closed when the last
 *    data is retrieved. You can resume this cursor later.
 * @MONGOC_QUERY_SLAVE_OK: Allow query of replica slave.
 * @MONGOC_QUERY_OPLOG_REPLAY: Used internally by Mongo.
 * @MONGOC_QUERY_NO_CURSOR_TIMEOUT: The server normally times out idle
 *    cursors after an inactivity period (10 minutes). This prevents that.
 * @MONGOC_QUERY_AWAIT_DATA: Use with %MONGOC_QUERY_TAILABLE_CURSOR. Block
 *    rather than returning no data. After a period, time out.
 * @MONGOC_QUERY_EXHAUST: Stream the data down full blast in multiple
 *    "more" packages. Faster when you are pulling a lot of data and
 *    know you want to pull it all down.
 * @MONGOC_QUERY_PARTIAL: Get partial results from mongos if some shards
 *    are down (instead of throwing an error).
 *
 * #mongoc_query_flags_t is used for querying a Mongo instance.
 */
typedef enum
{
   MONGOC_QUERY_NONE              = 0,
   MONGOC_QUERY_TAILABLE_CURSOR   = 1 << 1,
   MONGOC_QUERY_SLAVE_OK          = 1 << 2,
   MONGOC_QUERY_OPLOG_REPLAY      = 1 << 3,
   MONGOC_QUERY_NO_CURSOR_TIMEOUT = 1 << 4,
   MONGOC_QUERY_AWAIT_DATA        = 1 << 5,
   MONGOC_QUERY_EXHAUST           = 1 << 6,
   MONGOC_QUERY_PARTIAL           = 1 << 7,
} mongoc_query_flags_t;


/**
 * mongoc_reply_flags_t:
 * @MONGOC_REPLY_NONE: No flags set.
 * @MONGOC_REPLY_CURSOR_NOT_FOUND: Cursor was not found.
 * @MONGOC_REPLY_QUERY_FAILURE: Query failed, error document provided.
 * @MONGOC_REPLY_SHARD_CONFIG_STALE: Shard configuration is stale.
 * @MONGOC_REPLY_AWAIT_CAPABLE: Wait for data to be returned until timeout
 *    has passed. Used with %MONGOC_QUERY_TAILABLE_CURSOR.
 *
 * #mongoc_reply_flags_t contains flags supplied by the Mongo server in reply
 * to a request.
 */
typedef enum
{
   MONGOC_REPLY_NONE               = 0,
   MONGOC_REPLY_CURSOR_NOT_FOUND   = 1 << 0,
   MONGOC_REPLY_QUERY_FAILURE      = 1 << 1,
   MONGOC_REPLY_SHARD_CONFIG_STALE = 1 << 2,
   MONGOC_REPLY_AWAIT_CAPABLE      = 1 << 3,
} mongoc_reply_flags_t;


/**
 * mongoc_update_flags_t:
 * @MONGOC_UPDATE_NONE: No update flags specified.
 * @MONGOC_UPDATE_UPSERT: Perform an upsert.
 * @MONGOC_UPDATE_MULTI_UPDATE: Continue updating after first match.
 *
 * #mongoc_update_flags_t is used when updating documents found in Mongo.
 */
typedef enum
{
   MONGOC_UPDATE_NONE         = 0,
   MONGOC_UPDATE_UPSERT       = 1 << 0,
   MONGOC_UPDATE_MULTI_UPDATE = 1 << 1,
} mongoc_update_flags_t;


typedef enum
{
   MONGOC_OPCODE_REPLY         = 1,
   MONGOC_OPCODE_MSG           = 1000,
   MONGOC_OPCODE_UPDATE        = 2001,
   MONGOC_OPCODE_INSERT        = 2002,
   MONGOC_OPCODE_QUERY         = 2004,
   MONGOC_OPCODE_GET_MORE      = 2005,
   MONGOC_OPCODE_DELETE        = 2006,
   MONGOC_OPCODE_KILL_CURSORS  = 2007,
} mongoc_opcode_t;


typedef struct
{
   mongoc_opcode_t type;
#pragma pack(push, 1)
   bson_uint32_t   len;
   bson_int32_t    request_id;
   bson_int32_t    response_to;
   bson_uint32_t   opcode;
#pragma pack(pop)
} mongoc_event_any_t;


typedef struct
{
   mongoc_event_any_t     any;
   bson_uint32_t          zero;
   bson_uint32_t          nslen;
   const char            *ns;
   mongoc_update_flags_t  flags;
   bson_t                *selector;
   bson_t                *update;
} mongoc_event_update_t;


typedef struct
{
   mongoc_event_any_t      any;
   mongoc_insert_flags_t   flags;
   bson_uint32_t           nslen;
   const char             *ns;
   bson_uint32_t           docslen;
   bson_t                **docs;
} mongoc_event_insert_t;


typedef struct
{
   mongoc_event_any_t    any;
   mongoc_query_flags_t  flags;
   bson_uint32_t         nslen;
   const char           *ns;
   bson_uint32_t         skip;
   bson_uint32_t         n_return;
   bson_t               *query;
   bson_t               *fields;
} mongoc_event_query_t;


typedef struct
{
   mongoc_event_any_t  any;
   bson_uint32_t       zero;
   bson_uint32_t       nslen;
   const char         *ns;
   bson_uint32_t       n_return;
   bson_uint64_t       cursor_id;
} mongoc_event_get_more_t;


typedef struct
{
   mongoc_event_any_t     any;
   bson_uint32_t          zero;
   bson_uint32_t          nslen;
   const char            *ns;
   mongoc_delete_flags_t  flags;
   bson_t                *selector;
} mongoc_event_delete_t;


typedef struct
{
   mongoc_event_any_t  any;
   bson_uint32_t       zero;
   bson_uint32_t       n_cursors;
   bson_uint64_t      *cursors;
} mongoc_event_kill_cursors_t;


typedef struct
{
   mongoc_event_any_t  any;
   bson_uint32_t       msglen;
   const char         *msg;
} mongoc_event_msg_t;


typedef struct
{
   mongoc_event_any_t   any;
   bson_uint32_t        flags;
   bson_uint64_t        cursor_id;
   bson_uint32_t        start_from;
   bson_uint32_t        n_returned;
   bson_uint32_t        docslen;
   bson_t             **docs;
} mongoc_event_reply_t;


typedef union
{
   mongoc_opcode_t              type;
   mongoc_event_any_t           any;
   mongoc_event_delete_t        delete;
   mongoc_event_get_more_t      get_more;
   mongoc_event_insert_t        insert;
   mongoc_event_kill_cursors_t  kill_cursors;
   mongoc_event_msg_t           msg;
   mongoc_event_query_t         query;
   mongoc_event_reply_t         reply;
   mongoc_event_update_t        update;
} mongoc_event_t;


mongoc_client_t *
mongoc_client_new (const char *uri_string);


mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri);


bson_bool_t
mongoc_event_encode (mongoc_event_t     *event,
                     bson_uint8_t      **buf,
                     size_t             *buflen,
                     bson_realloc_func   realloc_func,
                     bson_error_t       *error);


bson_bool_t
mongoc_event_decode (mongoc_event_t     *event,
                     const bson_uint8_t *buf,
                     size_t              buflen,
                     bson_error_t       *error);


bson_bool_t
mongoc_event_write (mongoc_event_t *event,
                    int             sd,
                    bson_error_t   *error);


bson_bool_t
mongoc_event_read (mongoc_event_t *oper,
                   int             sd,
                   bson_error_t   *error);


bson_bool_t
mongoc_client_send (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_error_t    *error);


bson_bool_t
mongoc_client_recv (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_error_t    *error);


BSON_END_DECLS


#endif /* MONGOC_H */
