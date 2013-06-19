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


#ifndef MONGOC_RPC_PRIVATE_H
#define MONGOC_RPC_PRIVATE_H


#include <bson.h>

#include "mongoc-array-private.h"


BSON_BEGIN_DECLS


#define RPC(_name, _code)                typedef struct { _code } mongoc_rpc_##_name##_t;
#define INT32_FIELD(_name)               bson_int32_t _name;
#define INT64_FIELD(_name)               bson_int64_t _name;
#define INT64_ARRAY_FIELD(_len, _name)   bson_int32_t _len; bson_int64_t *_name;
#define CSTRING_FIELD(_name)             char _name[256];
#define BSON_FIELD(_name)                const bson_t *_name;
#define BSON_ARRAY_FIELD(_len, _name)    const bson_t * const *_name;
#define RAW_BUFFER_FIELD(_name)          void *_name; bson_int32_t _name##_len;
#define OPTIONAL(_check, _code)          _code


#pragma pack(push, 1)

#include "op-delete.def"
#include "op-get-more.def"
#include "op-header.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"

typedef union
{
   mongoc_rpc_delete_t       delete;
   mongoc_rpc_get_more_t     get_more;
   mongoc_rpc_header_t       header;
   mongoc_rpc_insert_t       insert;
   mongoc_rpc_kill_cursors_t kill_cursors;
   mongoc_rpc_msg_t          msg;
   mongoc_rpc_query_t        query;
   mongoc_rpc_reply_t        reply;
   mongoc_rpc_update_t       update;
} mongoc_rpc_t;

#pragma pack(pop)


#undef RPC
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef OPTIONAL
#undef RAW_BUFFER_FIELD


void mongoc_rpc_gather (mongoc_rpc_t   *rpc,
                        mongoc_array_t *array);
void mongoc_rpc_swab   (mongoc_rpc_t   *rpc);
void mongoc_rpc_printf (mongoc_rpc_t   *rpc);


BSON_END_DECLS


#endif /* MONGOC_RPC_PRIVATE_H */
