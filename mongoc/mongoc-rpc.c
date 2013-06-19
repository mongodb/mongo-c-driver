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


#include <sys/uio.h>

#include "mongoc-event-private.h"
#include "mongoc-log.h"
#include "mongoc-rpc-private.h"


#define RPC(_name, _code) \
   static BSON_INLINE void \
   mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                              mongoc_array_t *array) \
   { \
      struct iovec iov; \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(array); \
      _code \
   }
#define INT32_FIELD(_name) \
   iov.iov_base = &rpc->_name; \
   iov.iov_len = 4; \
   mongoc_array_append_val(array, iov);
#define INT64_FIELD(_name) \
   iov.iov_base = &rpc->_name; \
   iov.iov_len = 8; \
   mongoc_array_append_val(array, iov);
#define CSTRING_FIELD(_name) \
   iov.iov_base = rpc->_name; \
   iov.iov_len = strlen(rpc->_name) + 1; \
   mongoc_array_append_val(array, iov);
#define BSON_FIELD(_name) \
   iov.iov_base = (void *)bson_get_data(rpc->_name); \
   iov.iov_len = rpc->_name->len; \
   mongoc_array_append_val(array, iov);
#define OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define BSON_ARRAY_FIELD(_len, _name) \
   do { \
      typeof(rpc->_len) i; \
      for (i = 0; i < rpc->_len; i++) { \
         iov.iov_base = (void *)bson_get_data(rpc->_name[i]); \
         iov.iov_len = rpc->_name[i]->len; \
         mongoc_array_append_val(array, iov); \
      } \
   } while (0);
#define RAW_BUFFER_FIELD(_name) \
   iov.iov_base = rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   mongoc_array_append_val(array, iov);
#define INT64_ARRAY_FIELD(_len, _name) \
   iov.iov_base = rpc->_name; \
   iov.iov_len = rpc->_len * 8; \
   mongoc_array_append_val(array, iov);



#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef RAW_BUFFER_FIELD
#undef OPTIONAL


#define RPC(_name, _code) \
   static BSON_INLINE void \
   mongoc_rpc_swab_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   rpc->_name = BSON_UINT32_FROM_LE(rpc->_name);
#define INT64_FIELD(_name) \
   rpc->_name = BSON_UINT64_FROM_LE(rpc->_name);
#define CSTRING_FIELD(_name)
#define BSON_FIELD(_name)
#define BSON_ARRAY_FIELD(_len, _name) \
   rpc->_len = BSON_UINT32_FROM_LE(rpc->_len);
#define OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name)
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      typeof(rpc->_len) i; \
      for (i = 0; i < rpc->_len; i++) { \
         rpc->_name[i] = BSON_UINT64_FROM_LE(rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-header.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef OPTIONAL
#undef RAW_BUFFER_FIELD


#define RPC(_name, _code) \
   static BSON_INLINE void \
   mongoc_rpc_printf_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   printf("  "#_name" : %d\n", rpc->_name);
#define INT64_FIELD(_name) \
   printf("  "#_name" : %lld\n", (long long)rpc->_name);
#define CSTRING_FIELD(_name) \
   printf("  "#_name" : %s\n", rpc->_name);
#define BSON_FIELD(_name) \
   do { \
      char *s = bson_as_json(rpc->_name, NULL); \
      printf("  "#_name" : %s\n", s); \
      bson_free(s); \
   } while (0);
#define BSON_ARRAY_FIELD(_len, _name) \
   do { \
      typeof(rpc->_len) i; \
      for (i = 0; i < rpc->_len; i++) { \
         char *s = bson_as_json(rpc->_name[i], NULL); \
         printf("  "#_name"[%llu] : %s\n", (unsigned long long)i, s); \
         bson_free(s); \
      } \
   } while (0);
#define OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name)
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      typeof(rpc->_len) i; \
      for (i = 0; i < rpc->_len; i++) { \
         printf("  "#_name" : %lld\n", (long long)rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-header.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef OPTIONAL
#undef RAW_BUFFER_FIELD


void
mongoc_rpc_gather (mongoc_rpc_t   *rpc,
                   mongoc_array_t *array)
{
   bson_return_if_fail(rpc);
   bson_return_if_fail(array);

   switch ((mongoc_opcode_t)rpc->header.op_code) {
   case MONGOC_OPCODE_REPLY:
      return mongoc_rpc_gather_reply(&rpc->reply, array);
   case MONGOC_OPCODE_MSG:
      return mongoc_rpc_gather_msg(&rpc->msg, array);
   case MONGOC_OPCODE_UPDATE:
      return mongoc_rpc_gather_update(&rpc->update, array);
   case MONGOC_OPCODE_INSERT:
      return mongoc_rpc_gather_insert(&rpc->insert, array);
   case MONGOC_OPCODE_QUERY:
      return mongoc_rpc_gather_query(&rpc->query, array);
   case MONGOC_OPCODE_GET_MORE:
      return mongoc_rpc_gather_get_more(&rpc->get_more, array);
   case MONGOC_OPCODE_DELETE:
      return mongoc_rpc_gather_delete(&rpc->delete, array);
   case MONGOC_OPCODE_KILL_CURSORS:
      return mongoc_rpc_gather_kill_cursors(&rpc->kill_cursors, array);
   default:
      break;
   }
}


void
mongoc_rpc_swab (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.op_code) {
   case MONGOC_OPCODE_REPLY:
      mongoc_rpc_swab_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_rpc_swab_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_rpc_swab_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_rpc_swab_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_rpc_swab_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_rpc_swab_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      mongoc_rpc_swab_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_rpc_swab_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.op_code);
      break;
   }
}


void
mongoc_rpc_printf (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.op_code) {
   case MONGOC_OPCODE_REPLY:
      mongoc_rpc_printf_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      mongoc_rpc_printf_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      mongoc_rpc_printf_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      mongoc_rpc_printf_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      mongoc_rpc_printf_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      mongoc_rpc_printf_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      mongoc_rpc_printf_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_rpc_printf_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.op_code);
      break;
   }
}
