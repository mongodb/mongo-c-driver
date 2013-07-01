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

#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-rpc-private.h"


#define RPC(_name, _code) \
   static BSON_INLINE void \
   mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                              mongoc_array_t *array) \
   { \
      struct iovec iov; \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(array); \
      rpc->msg_len = 0; \
      _code \
   }
#define INT32_FIELD(_name) \
   iov.iov_base = &rpc->_name; \
   iov.iov_len = 4; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov);
#define INT64_FIELD(_name) \
   iov.iov_base = &rpc->_name; \
   iov.iov_len = 8; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov);
#define CSTRING_FIELD(_name) \
   assert(rpc->_name); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = strlen(rpc->_name) + 1; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov);
#define BSON_FIELD(_name) \
   do { \
      bson_int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = __l; \
      rpc->msg_len += iov.iov_len; \
      mongoc_array_append_val(array, iov); \
   } while (0);
#define OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define BSON_ARRAY_FIELD(_name) \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov);
#define RAW_BUFFER_FIELD(_name) \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov);
#define INT64_ARRAY_FIELD(_len, _name) \
   iov.iov_base = &rpc->_len; \
   iov.iov_len = 4; \
   rpc->msg_len += iov.iov_len; \
   mongoc_array_append_val(array, iov); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_len * 8; \
   rpc->msg_len += iov.iov_len; \
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
#define BSON_ARRAY_FIELD(_name) \
   rpc->_name##_len = BSON_UINT32_FROM_LE(rpc->_name##_len);
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
      bson_t b; \
      char *s; \
      bson_int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      bson_init_static(&b, rpc->_name, __l); \
      s = bson_as_json(&b, NULL); \
      printf("  "#_name" : %s\n", s); \
      bson_free(s); \
      bson_destroy(&b); \
   } while (0);
#define BSON_ARRAY_FIELD(_name) \
   do { \
      bson_reader_t __r; \
      bson_bool_t __eof; \
      const bson_t *__b; \
      bson_reader_init_from_data(&__r, rpc->_name, rpc->_name##_len); \
      while ((__b = bson_reader_read(&__r, &__eof))) { \
         char *s = bson_as_json(__b, NULL); \
         printf("  "#_name" : %s\n", s); \
         bson_free(s); \
      } \
   } while (0);
#define OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name) \
   size_t __i; \
   printf("  "#_name" :"); \
   for (__i = 0; __i < rpc->_name##_len; __i++) { \
      bson_uint8_t u; \
      u = ((char *)rpc->_name)[__i]; \
      printf(" %02x", u); \
   } \
   printf("\n");
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


#define RPC(_name, _code) \
   static bson_bool_t \
   mongoc_rpc_scatter_##_name (mongoc_rpc_##_name##_t *rpc, \
                               const bson_uint8_t *buf, \
                               size_t buflen) \
   { \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(buf); \
      BSON_ASSERT(buflen); \
      _code \
      return TRUE; \
   }
#define INT32_FIELD(_name) \
   if (buflen < 4) { \
      return FALSE; \
   } \
   memcpy(&rpc->_name, buf, 4); \
   buflen -= 4; \
   buf += 4;
#define INT64_FIELD(_name) \
   if (buflen < 8) { \
      return FALSE; \
   } \
   memcpy(&rpc->_name, buf, 8); \
   buflen -= 8; \
   buf += 8;
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      size_t needed; \
      if (buflen < 4) { \
         return FALSE; \
      } \
      memcpy(&rpc->_len, buf, 4); \
      buflen -= 4; \
      buf += 4; \
      needed = BSON_UINT32_FROM_LE(rpc->_len) * 8; \
      if (needed > buflen) { \
         return FALSE; \
      } \
      rpc->_name = (void *)buf; \
      buf += needed; \
      buflen -= needed; \
   } while (0);
#define CSTRING_FIELD(_name) \
   do { \
      size_t __i; \
      bson_bool_t found = FALSE; \
      for (__i = 0; __i < buflen; __i++) { \
         if (!buf[__i]) { \
            rpc->_name = (const char *)buf; \
            buflen -= __i + 1; \
            buf += __i + 1; \
            found = TRUE; \
            break; \
         } \
      } \
      if (!found) { \
         return FALSE; \
      } \
   } while (0);
#define BSON_FIELD(_name) \
   do { \
      bson_int32_t __l; \
      if (buflen < 4) { \
         return FALSE; \
      } \
      memcpy(&__l, buf, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      if (__l < 5 || __l > buflen) { \
         return FALSE; \
      } \
      rpc->_name = (bson_uint8_t *)buf; \
      buf += __l; \
      buflen -= __l; \
   } while (0);
#define BSON_ARRAY_FIELD(_name) \
   rpc->_name = (bson_uint8_t *)buf; \
   rpc->_name##_len = buflen; \
   buf = NULL; \
   buflen = 0;
#define OPTIONAL(_check, _code) \
   if (buflen) { \
      _code \
   }
#define RAW_BUFFER_FIELD(_name) \
   rpc->_name = (void *)buf; \
   rpc->_name##_len = buflen; \
   buf = NULL; \
   buflen = 0;


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

   switch ((mongoc_opcode_t)rpc->header.opcode) {
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
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


void
mongoc_rpc_swab (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
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
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


void
mongoc_rpc_printf (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
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
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


bson_bool_t
mongoc_rpc_scatter (mongoc_rpc_t       *rpc,
                    const bson_uint8_t *buf,
                    size_t              buflen)
{
   bson_return_val_if_fail(rpc, FALSE);
   bson_return_val_if_fail(buf, FALSE);
   bson_return_val_if_fail(buflen, FALSE);

   if (BSON_UNLIKELY(buflen < 16)) {
      return FALSE;
   }

   mongoc_rpc_scatter_header(&rpc->header, buf, 16);
   mongoc_rpc_swab_header(&rpc->header);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      return mongoc_rpc_scatter_reply(&rpc->reply, buf, buflen);
   case MONGOC_OPCODE_MSG:
      return mongoc_rpc_scatter_msg(&rpc->msg, buf, buflen);
   case MONGOC_OPCODE_UPDATE:
      return mongoc_rpc_scatter_update(&rpc->update, buf, buflen);
   case MONGOC_OPCODE_INSERT:
      return mongoc_rpc_scatter_insert(&rpc->insert, buf, buflen);
   case MONGOC_OPCODE_QUERY:
      return mongoc_rpc_scatter_query(&rpc->query, buf, buflen);
   case MONGOC_OPCODE_GET_MORE:
      return mongoc_rpc_scatter_get_more(&rpc->get_more, buf, buflen);
   case MONGOC_OPCODE_DELETE:
      return mongoc_rpc_scatter_delete(&rpc->delete, buf, buflen);
   case MONGOC_OPCODE_KILL_CURSORS:
      return mongoc_rpc_scatter_kill_cursors(&rpc->kill_cursors, buf, buflen);
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      return FALSE;
   }
}


bson_bool_t
mongoc_rpc_reply_get_first (mongoc_rpc_reply_t *reply,
                            bson_t             *bson)
{
   bson_int32_t len;

   bson_return_val_if_fail(reply, FALSE);
   bson_return_val_if_fail(bson, FALSE);

   if (!reply->documents || reply->documents_len < 4) {
      return FALSE;
   }

   memcpy(&len, reply->documents, 4);
   len = BSON_UINT32_FROM_LE(len);
   if (reply->documents_len < len) {
      return FALSE;
   }

   return bson_init_static(bson, reply->documents, len);
}
