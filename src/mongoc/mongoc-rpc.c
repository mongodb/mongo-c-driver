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


#include <bson.h>

#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-rpc-private.h"


#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                               mongoc_array_t *array) \
   { \
      mongoc_iovec_t iov; \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(array); \
      rpc->msg_len = 0; \
      _code \
   }
#define INT32_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 4; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   iov.iov_base = (void *)&rpc->_name; \
   iov.iov_len = 8; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define CSTRING_FIELD(_name) \
   BSON_ASSERT(rpc->_name); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = strlen(rpc->_name) + 1; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define BSON_FIELD(_name) \
   do { \
      int32_t __l; \
      memcpy(&__l, rpc->_name, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = __l; \
      BSON_ASSERT(iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define BSON_ARRAY_FIELD(_name) \
   if (rpc->_name##_len) { \
      iov.iov_base = (void *)rpc->_name; \
      iov.iov_len = rpc->_name##_len; \
      BSON_ASSERT(iov.iov_len); \
      rpc->msg_len += (int32_t)iov.iov_len; \
      _mongoc_array_append_val(array, iov); \
   }
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      ssize_t _i; \
      BSON_ASSERT(rpc->n_##_name); \
      for (_i = 0; _i < rpc->n_##_name; _i++) { \
         BSON_ASSERT(rpc->_name[_i].iov_len); \
         rpc->msg_len += (int32_t)rpc->_name[_i].iov_len; \
         _mongoc_array_append_val(array, rpc->_name[_i]); \
      } \
   } while (0);
#define RAW_BUFFER_FIELD(_name) \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_name##_len; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);
#define INT64_ARRAY_FIELD(_len, _name) \
   iov.iov_base = (void *)&rpc->_len; \
   iov.iov_len = 4; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov); \
   iov.iov_base = (void *)rpc->_name; \
   iov.iov_len = rpc->_len * 8; \
   BSON_ASSERT(iov.iov_len); \
   rpc->msg_len += (int32_t)iov.iov_len; \
   _mongoc_array_append_val(array, iov);



#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef RAW_BUFFER_FIELD
#undef BSON_OPTIONAL


#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_swab_to_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   rpc->_name = BSON_UINT32_FROM_LE(rpc->_name);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   rpc->_name = BSON_UINT64_FROM_LE(rpc->_name);
#define CSTRING_FIELD(_name)
#define BSON_FIELD(_name)
#define BSON_ARRAY_FIELD(_name)
#define IOVEC_ARRAY_FIELD(_name)
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name)
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      for (i = 0; i < rpc->_len; i++) { \
         rpc->_name[i] = BSON_UINT64_FROM_LE(rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"

#undef RPC
#undef INT64_ARRAY_FIELD

#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_swab_from_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
      for (i = 0; i < rpc->_len; i++) { \
         rpc->_name[i] = BSON_UINT64_FROM_LE(rpc->_name[i]); \
      } \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD

#endif /* BSON_BYTE_ORDER == BSON_BIG_ENDIAN */


#define RPC(_name, _code) \
   static void \
   _mongoc_rpc_printf_##_name (mongoc_rpc_##_name##_t *rpc) \
   { \
      BSON_ASSERT(rpc); \
      _code \
   }
#define INT32_FIELD(_name) \
   printf("  "#_name" : %d\n", rpc->_name);
#define ENUM_FIELD(_name) \
   printf("  "#_name" : %u\n", rpc->_name);
#define INT64_FIELD(_name) \
   printf("  "#_name" : %" PRIi64 "\n", (int64_t)rpc->_name);
#define CSTRING_FIELD(_name) \
   printf("  "#_name" : %s\n", rpc->_name);
#define BSON_FIELD(_name) \
   do { \
      bson_t b; \
      char *s; \
      int32_t __l; \
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
      bson_reader_t *__r; \
      bool __eof; \
      const bson_t *__b; \
      __r = bson_reader_new_from_data(rpc->_name, rpc->_name##_len); \
      while ((__b = bson_reader_read(__r, &__eof))) { \
         char *s = bson_as_json(__b, NULL); \
         printf("  "#_name" : %s\n", s); \
         bson_free(s); \
      } \
      bson_reader_destroy(__r); \
   } while (0);
#define IOVEC_ARRAY_FIELD(_name) \
   do { \
      ssize_t _i; \
      size_t _j; \
      for (_i = 0; _i < rpc->n_##_name; _i++) { \
         printf("  "#_name" : "); \
         for (_j = 0; _j < rpc->_name[_i].iov_len; _j++) { \
            uint8_t u; \
            u = ((char *)rpc->_name[_i].iov_base)[_j]; \
            printf(" %02x", u); \
         } \
         printf("\n"); \
      } \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) { _code }
#define RAW_BUFFER_FIELD(_name) \
   { \
      ssize_t __i; \
      printf("  "#_name" :"); \
      for (__i = 0; __i < rpc->_name##_len; __i++) { \
         uint8_t u; \
         u = ((char *)rpc->_name)[__i]; \
         printf(" %02x", u); \
      } \
      printf("\n"); \
   }
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      ssize_t i; \
      for (i = 0; i < rpc->_len; i++) { \
         printf("  "#_name" : %" PRIi64 "\n", (int64_t)rpc->_name[i]); \
      } \
      rpc->_len = BSON_UINT32_FROM_LE(rpc->_len); \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


#define RPC(_name, _code) \
   static bool \
   _mongoc_rpc_scatter_##_name (mongoc_rpc_##_name##_t *rpc, \
                                const uint8_t *buf, \
                                size_t buflen) \
   { \
      BSON_ASSERT(rpc); \
      BSON_ASSERT(buf); \
      BSON_ASSERT(buflen); \
      _code \
      return true; \
   }
#define INT32_FIELD(_name) \
   if (buflen < 4) { \
      return false; \
   } \
   memcpy(&rpc->_name, buf, 4); \
   buflen -= 4; \
   buf += 4;
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) \
   if (buflen < 8) { \
      return false; \
   } \
   memcpy(&rpc->_name, buf, 8); \
   buflen -= 8; \
   buf += 8;
#define INT64_ARRAY_FIELD(_len, _name) \
   do { \
      size_t needed; \
      if (buflen < 4) { \
         return false; \
      } \
      memcpy(&rpc->_len, buf, 4); \
      buflen -= 4; \
      buf += 4; \
      needed = BSON_UINT32_FROM_LE(rpc->_len) * 8; \
      if (needed > buflen) { \
         return false; \
      } \
      rpc->_name = (void *)buf; \
      buf += needed; \
      buflen -= needed; \
   } while (0);
#define CSTRING_FIELD(_name) \
   do { \
      size_t __i; \
      bool found = false; \
      for (__i = 0; __i < buflen; __i++) { \
         if (!buf[__i]) { \
            rpc->_name = (const char *)buf; \
            buflen -= __i + 1; \
            buf += __i + 1; \
            found = true; \
            break; \
         } \
      } \
      if (!found) { \
         return false; \
      } \
   } while (0);
#define BSON_FIELD(_name) \
   do { \
      uint32_t __l; \
      if (buflen < 4) { \
         return false; \
      } \
      memcpy(&__l, buf, 4); \
      __l = BSON_UINT32_FROM_LE(__l); \
      if (__l < 5 || __l > buflen) { \
         return false; \
      } \
      rpc->_name = (uint8_t *)buf; \
      buf += __l; \
      buflen -= __l; \
   } while (0);
#define BSON_ARRAY_FIELD(_name) \
   rpc->_name = (uint8_t *)buf; \
   rpc->_name##_len = (int32_t)buflen; \
   buf = NULL; \
   buflen = 0;
#define BSON_OPTIONAL(_check, _code) \
   if (buflen) { \
      _code \
   }
#define IOVEC_ARRAY_FIELD(_name) \
   rpc->_name##_recv.iov_base = (void *)buf; \
   rpc->_name##_recv.iov_len = buflen; \
   rpc->_name = &rpc->_name##_recv; \
   rpc->n_##_name = 1; \
   buf = NULL; \
   buflen = 0;
#define RAW_BUFFER_FIELD(_name) \
   rpc->_name = (void *)buf; \
   rpc->_name##_len = (int32_t)buflen; \
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
#undef ENUM_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


void
_mongoc_rpc_gather (mongoc_rpc_t   *rpc,
                    mongoc_array_t *array)
{
   bson_return_if_fail(rpc);
   bson_return_if_fail(array);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_gather_reply(&rpc->reply, array);
      return;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_gather_msg(&rpc->msg, array);
      return;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_gather_update(&rpc->update, array);
      return;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_gather_insert(&rpc->insert, array);
      return;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_gather_query(&rpc->query, array);
      return;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_gather_get_more(&rpc->get_more, array);
      return;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_gather_delete(&rpc->delete, array);
      return;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_gather_kill_cursors(&rpc->kill_cursors, array);
      return;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


void
_mongoc_rpc_swab_to_le (mongoc_rpc_t *rpc)
{
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   mongoc_opcode_t opcode;

   bson_return_if_fail(rpc);

   opcode = rpc->header.opcode;

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_swab_to_le_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_swab_to_le_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_swab_to_le_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_swab_to_le_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_swab_to_le_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_swab_to_le_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_swab_to_le_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_swab_to_le_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", opcode);
      break;
   }
#endif
}


void
_mongoc_rpc_swab_from_le (mongoc_rpc_t *rpc)
{
#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   mongoc_opcode_t opcode;

   bson_return_if_fail(rpc);

   opcode = BSON_UINT32_FROM_LE(rpc->header.opcode);

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_swab_from_le_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_swab_from_le_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_swab_from_le_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_swab_from_le_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_swab_from_le_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_swab_from_le_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_swab_from_le_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_swab_from_le_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
#endif
}


void
_mongoc_rpc_printf (mongoc_rpc_t *rpc)
{
   bson_return_if_fail(rpc);

   switch ((mongoc_opcode_t)rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_printf_reply(&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_printf_msg(&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_printf_update(&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_printf_insert(&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_printf_query(&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_printf_get_more(&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_printf_delete(&rpc->delete);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_printf_kill_cursors(&rpc->kill_cursors);
      break;
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
}


bool
_mongoc_rpc_scatter (mongoc_rpc_t  *rpc,
                     const uint8_t *buf,
                     size_t         buflen)
{
   mongoc_opcode_t opcode;

   bson_return_val_if_fail(rpc, false);
   bson_return_val_if_fail(buf, false);
   bson_return_val_if_fail(buflen, false);

   memset (rpc, 0, sizeof *rpc);

   if (BSON_UNLIKELY(buflen < 16)) {
      return false;
   }

   if (!_mongoc_rpc_scatter_header(&rpc->header, buf, 16)) {
      return false;
   }

   opcode = BSON_UINT32_FROM_LE(rpc->header.opcode);

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      return _mongoc_rpc_scatter_reply(&rpc->reply, buf, buflen);
   case MONGOC_OPCODE_MSG:
      return _mongoc_rpc_scatter_msg(&rpc->msg, buf, buflen);
   case MONGOC_OPCODE_UPDATE:
      return _mongoc_rpc_scatter_update(&rpc->update, buf, buflen);
   case MONGOC_OPCODE_INSERT:
      return _mongoc_rpc_scatter_insert(&rpc->insert, buf, buflen);
   case MONGOC_OPCODE_QUERY:
      return _mongoc_rpc_scatter_query(&rpc->query, buf, buflen);
   case MONGOC_OPCODE_GET_MORE:
      return _mongoc_rpc_scatter_get_more(&rpc->get_more, buf, buflen);
   case MONGOC_OPCODE_DELETE:
      return _mongoc_rpc_scatter_delete(&rpc->delete, buf, buflen);
   case MONGOC_OPCODE_KILL_CURSORS:
      return _mongoc_rpc_scatter_kill_cursors(&rpc->kill_cursors, buf, buflen);
   default:
      MONGOC_WARNING("Unknown rpc type: 0x%08x", opcode);
      return false;
   }
}


bool
_mongoc_rpc_reply_get_first (mongoc_rpc_reply_t *reply,
                             bson_t             *bson)
{
   int32_t len;

   bson_return_val_if_fail(reply, false);
   bson_return_val_if_fail(bson, false);

   if (!reply->documents || reply->documents_len < 4) {
      return false;
   }

   memcpy(&len, reply->documents, 4);
   len = BSON_UINT32_FROM_LE(len);
   if (reply->documents_len < len) {
      return false;
   }

   return bson_init_static(bson, reply->documents, len);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_needs_gle --
 *
 *       Checks to see if an rpc requires a getlasterror command to
 *       determine the success of the rpc.
 *
 *       The write_concern is checked to ensure that the caller wants
 *       to know about a failure.
 *
 * Returns:
 *       true if a getlasterror should be delivered; otherwise false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_rpc_needs_gle (mongoc_rpc_t                 *rpc,
                       const mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(rpc, false);

   switch (rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
   case MONGOC_OPCODE_QUERY:
   case MONGOC_OPCODE_MSG:
   case MONGOC_OPCODE_GET_MORE:
   case MONGOC_OPCODE_KILL_CURSORS:
      return false;
   case MONGOC_OPCODE_INSERT:
   case MONGOC_OPCODE_UPDATE:
   case MONGOC_OPCODE_DELETE:
   default:
      break;
   }

   if (!write_concern || !mongoc_write_concern_get_w(write_concern)) {
      return false;
   }

   return true;
}
