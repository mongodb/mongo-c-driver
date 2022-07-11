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


#include <bson/bson.h>

#include "mongoc.h"
#include "mongoc-rpc-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-util-private.h"
#include "mongoc-compression-private.h"
#include "mongoc-cluster-private.h"


#define RPC(_name, _code)                                               \
   static void _mongoc_rpc_gather_##_name (mongoc_rpc_##_name##_t *rpc, \
                                           mongoc_rpc_header_t *header, \
                                           mongoc_array_t *array)       \
   {                                                                    \
      mongoc_iovec_t iov;                                               \
      BSON_ASSERT (rpc);                                                \
      BSON_ASSERT (array);                                              \
      header->msg_len = 0;                                              \
      _code                                                             \
   }
#define UINT8_FIELD(_name)                   \
   iov.iov_base = (void *) &rpc->_name;      \
   iov.iov_len = 1;                          \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);
#define INT32_FIELD(_name)                   \
   iov.iov_base = (void *) &rpc->_name;      \
   iov.iov_len = 4;                          \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name)                   \
   iov.iov_base = (void *) &rpc->_name;      \
   iov.iov_len = 8;                          \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);
#define CSTRING_FIELD(_name)                 \
   BSON_ASSERT (rpc->_name);                 \
   iov.iov_base = (void *) rpc->_name;       \
   iov.iov_len = strlen (rpc->_name) + 1;    \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);
#define BSON_FIELD(_name)                       \
   do {                                         \
      int32_t __l;                              \
      memcpy (&__l, rpc->_name, 4);             \
      __l = BSON_UINT32_FROM_LE (__l);          \
      iov.iov_base = (void *) rpc->_name;       \
      iov.iov_len = __l;                        \
      header->msg_len += (int32_t) iov.iov_len; \
      _mongoc_array_append_val (array, iov);    \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) {                \
      _code                          \
   }
#define BSON_ARRAY_FIELD(_name)                 \
   if (rpc->_name##_len) {                      \
      iov.iov_base = (void *) rpc->_name;       \
      iov.iov_len = rpc->_name##_len;           \
      header->msg_len += (int32_t) iov.iov_len; \
      _mongoc_array_append_val (array, iov);    \
   }
#define IOVEC_ARRAY_FIELD(_name)                              \
   do {                                                       \
      ssize_t _i;                                             \
      BSON_ASSERT (rpc->n_##_name);                           \
      for (_i = 0; _i < rpc->n_##_name; _i++) {               \
         BSON_ASSERT (rpc->_name[_i].iov_len);                \
         header->msg_len += (int32_t) rpc->_name[_i].iov_len; \
         _mongoc_array_append_val (array, rpc->_name[_i]);    \
      }                                                       \
   } while (0);
#define SECTION_ARRAY_FIELD(_name)                                            \
   do {                                                                       \
      ssize_t _i;                                                             \
      BSON_ASSERT (rpc->n_##_name);                                           \
      for (_i = 0; _i < rpc->n_##_name; _i++) {                               \
         int32_t __l;                                                         \
         iov.iov_base = (void *) &rpc->_name[_i].payload_type;                \
         iov.iov_len = 1;                                                     \
         header->msg_len += (int32_t) iov.iov_len;                            \
         _mongoc_array_append_val (array, iov);                               \
         switch (rpc->_name[_i].payload_type) {                               \
         case 0:                                                              \
            memcpy (&__l, rpc->_name[_i].payload.bson_document, 4);           \
            __l = BSON_UINT32_FROM_LE (__l);                                  \
            iov.iov_base = (void *) rpc->_name[_i].payload.bson_document;     \
            iov.iov_len = __l;                                                \
            break;                                                            \
         case 1:                                                              \
            rpc->_name[_i].payload.sequence.size_le =                         \
               BSON_UINT32_TO_LE (rpc->_name[_i].payload.sequence.size);      \
            iov.iov_base = (void *) &rpc->_name[_i].payload.sequence.size_le; \
            iov.iov_len = 4;                                                  \
            header->msg_len += 4;                                             \
            _mongoc_array_append_val (array, iov);                            \
            iov.iov_base =                                                    \
               (void *) rpc->_name[_i].payload.sequence.identifier;           \
            iov.iov_len =                                                     \
               strlen (rpc->_name[_i].payload.sequence.identifier) + 1;       \
            header->msg_len += (int32_t) iov.iov_len;                         \
            _mongoc_array_append_val (array, iov);                            \
            iov.iov_base =                                                    \
               (void *) rpc->_name[_i].payload.sequence.bson_documents;       \
            iov.iov_len =                                                     \
               rpc->_name[_i].payload.sequence.size - iov.iov_len - 4;        \
            break;                                                            \
         default:                                                             \
            MONGOC_ERROR ("Unknown Payload Type: %d",                         \
                          rpc->_name[_i].payload_type);                       \
            BSON_ASSERT (0);                                                  \
         }                                                                    \
         header->msg_len += (int32_t) iov.iov_len;                            \
         _mongoc_array_append_val (array, iov);                               \
      }                                                                       \
   } while (0);
#define RAW_BUFFER_FIELD(_name)              \
   iov.iov_base = (void *) rpc->_name;       \
   iov.iov_len = rpc->_name##_len;           \
   BSON_ASSERT (iov.iov_len);                \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);
#define INT64_ARRAY_FIELD(_len, _name)       \
   iov.iov_base = (void *) &rpc->_len;       \
   iov.iov_len = 4;                          \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);    \
   iov.iov_base = (void *) rpc->_name;       \
   iov.iov_len = rpc->_len * 8;              \
   BSON_ASSERT (iov.iov_len);                \
   header->msg_len += (int32_t) iov.iov_len; \
   _mongoc_array_append_val (array, iov);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-compressed.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef UINT8_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef SECTION_ARRAY_FIELD
#undef RAW_BUFFER_FIELD
#undef BSON_OPTIONAL


#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN

#define RPC(_name, _code)                                                   \
   static void _mongoc_rpc_swab_to_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   {                                                                        \
      BSON_ASSERT (rpc);                                                    \
      _code                                                                 \
   }
#define UINT8_FIELD(_name)
#define INT32_FIELD(_name) rpc->_name = BSON_UINT32_FROM_LE (rpc->_name);
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name) rpc->_name = BSON_UINT64_FROM_LE (rpc->_name);
#define CSTRING_FIELD(_name)
#define BSON_FIELD(_name)
#define BSON_ARRAY_FIELD(_name)
#define IOVEC_ARRAY_FIELD(_name)
#define SECTION_ARRAY_FIELD(_name)
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) {                \
      _code                          \
   }
#define RAW_BUFFER_FIELD(_name)
#define INT64_ARRAY_FIELD(_len, _name)                        \
   do {                                                       \
      ssize_t i;                                              \
      for (i = 0; i < rpc->_len; i++) {                       \
         rpc->_name[i] = BSON_UINT64_FROM_LE (rpc->_name[i]); \
      }                                                       \
      rpc->_len = BSON_UINT32_FROM_LE (rpc->_len);            \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-compressed.def"
#include "op-update.def"

#undef RPC
#undef INT64_ARRAY_FIELD

#define RPC(_name, _code)                                                     \
   static void _mongoc_rpc_swab_from_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   {                                                                          \
      BSON_ASSERT (rpc);                                                      \
      _code                                                                   \
   }
#define INT64_ARRAY_FIELD(_len, _name)                        \
   do {                                                       \
      ssize_t i;                                              \
      rpc->_len = BSON_UINT32_FROM_LE (rpc->_len);            \
      for (i = 0; i < rpc->_len; i++) {                       \
         rpc->_name[i] = BSON_UINT64_FROM_LE (rpc->_name[i]); \
      }                                                       \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-compressed.def"
#include "op-update.def"

#undef RPC
#undef ENUM_FIELD
#undef UINT8_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef SECTION_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD

#endif /* BSON_BYTE_ORDER == BSON_BIG_ENDIAN */


#define RPC(_name, _code)                                               \
   static void _mongoc_rpc_printf_##_name (mongoc_rpc_##_name##_t *rpc) \
   {                                                                    \
      BSON_ASSERT (rpc);                                                \
      _code                                                             \
   }
#define UINT8_FIELD(_name) printf ("  " #_name " : %u\n", rpc->_name);
#define INT32_FIELD(_name) printf ("  " #_name " : %d\n", rpc->_name);
#define ENUM_FIELD(_name) printf ("  " #_name " : %u\n", rpc->_name);
#define INT64_FIELD(_name) \
   printf ("  " #_name " : %" PRIi64 "\n", (int64_t) rpc->_name);
#define CSTRING_FIELD(_name) printf ("  " #_name " : %s\n", rpc->_name);
#define BSON_FIELD(_name)                                   \
   do {                                                     \
      bson_t b;                                             \
      char *s;                                              \
      int32_t __l;                                          \
      memcpy (&__l, rpc->_name, 4);                         \
      __l = BSON_UINT32_FROM_LE (__l);                      \
      BSON_ASSERT (bson_init_static (&b, rpc->_name, __l)); \
      s = bson_as_relaxed_extended_json (&b, NULL);         \
      printf ("  " #_name " : %s\n", s);                    \
      bson_free (s);                                        \
      bson_destroy (&b);                                    \
   } while (0);
#define BSON_ARRAY_FIELD(_name)                                       \
   do {                                                               \
      bson_reader_t *__r;                                             \
      bool __eof;                                                     \
      const bson_t *__b;                                              \
      __r = bson_reader_new_from_data (rpc->_name, rpc->_name##_len); \
      while ((__b = bson_reader_read (__r, &__eof))) {                \
         char *s = bson_as_relaxed_extended_json (__b, NULL);         \
         printf ("  " #_name " : %s\n", s);                           \
         bson_free (s);                                               \
      }                                                               \
      bson_reader_destroy (__r);                                      \
   } while (0);
#define IOVEC_ARRAY_FIELD(_name)                           \
   do {                                                    \
      ssize_t _i;                                          \
      size_t _j;                                           \
      for (_i = 0; _i < rpc->n_##_name; _i++) {            \
         printf ("  " #_name " : ");                       \
         for (_j = 0; _j < rpc->_name[_i].iov_len; _j++) { \
            uint8_t u;                                     \
            u = ((char *) rpc->_name[_i].iov_base)[_j];    \
            printf (" %02x", u);                           \
         }                                                 \
         printf ("\n");                                    \
      }                                                    \
   } while (0);
#define SECTION_ARRAY_FIELD(_name)                                          \
   do {                                                                     \
      ssize_t _i;                                                           \
      printf ("  " #_name " : %d\n", rpc->n_##_name);                       \
      for (_i = 0; _i < rpc->n_##_name; _i++) {                             \
         if (rpc->_name[_i].payload_type == 0) {                            \
            do {                                                            \
               bson_t b;                                                    \
               char *s;                                                     \
               int32_t __l;                                                 \
               memcpy (&__l, rpc->_name[_i].payload.bson_document, 4);      \
               __l = BSON_UINT32_FROM_LE (__l);                             \
               BSON_ASSERT (bson_init_static (                              \
                  &b, rpc->_name[_i].payload.bson_document, __l));          \
               s = bson_as_relaxed_extended_json (&b, NULL);                \
               printf ("  Type %d: %s\n", rpc->_name[_i].payload_type, s);  \
               bson_free (s);                                               \
               bson_destroy (&b);                                           \
            } while (0);                                                    \
         } else if (rpc->_name[_i].payload_type == 1) {                     \
            bson_reader_t *__r;                                             \
            int max = rpc->_name[_i].payload.sequence.size -                \
                      strlen (rpc->_name[_i].payload.sequence.identifier) - \
                      1 - sizeof (int32_t);                                 \
            bool __eof;                                                     \
            const bson_t *__b;                                              \
            printf ("  Identifier: %s\n",                                   \
                    rpc->_name[_i].payload.sequence.identifier);            \
            printf ("  Size: %d\n", max);                                   \
            __r = bson_reader_new_from_data (                               \
               rpc->_name[_i].payload.sequence.bson_documents, max);        \
            while ((__b = bson_reader_read (__r, &__eof))) {                \
               char *s = bson_as_relaxed_extended_json (__b, NULL);         \
               bson_free (s);                                               \
            }                                                               \
            bson_reader_destroy (__r);                                      \
         }                                                                  \
      }                                                                     \
   } while (0);
#define BSON_OPTIONAL(_check, _code) \
   if (rpc->_check) {                \
      _code                          \
   }
#define RAW_BUFFER_FIELD(_name)                      \
   {                                                 \
      ssize_t __i;                                   \
      printf ("  " #_name " :");                     \
      for (__i = 0; __i < rpc->_name##_len; __i++) { \
         uint8_t u;                                  \
         u = ((char *) rpc->_name)[__i];             \
         printf (" %02x", u);                        \
      }                                              \
      printf ("\n");                                 \
   }
#define INT64_ARRAY_FIELD(_len, _name)                                     \
   do {                                                                    \
      ssize_t i;                                                           \
      for (i = 0; i < rpc->_len; i++) {                                    \
         printf ("  " #_name " : %" PRIi64 "\n", (int64_t) rpc->_name[i]); \
      }                                                                    \
      rpc->_len = BSON_UINT32_FROM_LE (rpc->_len);                         \
   } while (0);


#include "op-delete.def"
#include "op-get-more.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-compressed.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef UINT8_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef SECTION_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


#define RPC(_name, _code)                                             \
   static bool _mongoc_rpc_scatter_##_name (                          \
      mongoc_rpc_##_name##_t *rpc, const uint8_t *buf, size_t buflen) \
   {                                                                  \
      BSON_ASSERT (rpc);                                              \
      BSON_ASSERT (buf);                                              \
      BSON_ASSERT (buflen);                                           \
      _code return true;                                              \
   }
#define UINT8_FIELD(_name)       \
   if (buflen < 1) {             \
      return false;              \
   }                             \
   memcpy (&rpc->_name, buf, 1); \
   buflen -= 1;                  \
   buf += 1;
#define INT32_FIELD(_name)       \
   if (buflen < 4) {             \
      return false;              \
   }                             \
   memcpy (&rpc->_name, buf, 4); \
   buflen -= 4;                  \
   buf += 4;
#define ENUM_FIELD INT32_FIELD
#define INT64_FIELD(_name)       \
   if (buflen < 8) {             \
      return false;              \
   }                             \
   memcpy (&rpc->_name, buf, 8); \
   buflen -= 8;                  \
   buf += 8;
#define INT64_ARRAY_FIELD(_len, _name)              \
   do {                                             \
      size_t needed;                                \
      if (buflen < 4) {                             \
         return false;                              \
      }                                             \
      memcpy (&rpc->_len, buf, 4);                  \
      buflen -= 4;                                  \
      buf += 4;                                     \
      needed = BSON_UINT32_FROM_LE (rpc->_len) * 8; \
      if (needed > buflen) {                        \
         return false;                              \
      }                                             \
      rpc->_name = (int64_t *) buf;                 \
      buf += needed;                                \
      buflen -= needed;                             \
   } while (0);
#define CSTRING_FIELD(_name)                 \
   do {                                      \
      size_t __i;                            \
      bool found = false;                    \
      for (__i = 0; __i < buflen; __i++) {   \
         if (!buf[__i]) {                    \
            rpc->_name = (const char *) buf; \
            buflen -= __i + 1;               \
            buf += __i + 1;                  \
            found = true;                    \
            break;                           \
         }                                   \
      }                                      \
      if (!found) {                          \
         return false;                       \
      }                                      \
   } while (0);
#define BSON_FIELD(_name)              \
   do {                                \
      uint32_t __l;                    \
      if (buflen < 4) {                \
         return false;                 \
      }                                \
      memcpy (&__l, buf, 4);           \
      __l = BSON_UINT32_FROM_LE (__l); \
      if (__l < 5 || __l > buflen) {   \
         return false;                 \
      }                                \
      rpc->_name = (uint8_t *) buf;    \
      buf += __l;                      \
      buflen -= __l;                   \
   } while (0);
#define BSON_ARRAY_FIELD(_name)         \
   rpc->_name = (uint8_t *) buf;        \
   rpc->_name##_len = (int32_t) buflen; \
   buf = NULL;                          \
   buflen = 0;
#define BSON_OPTIONAL(_check, _code) \
   if (buflen) {                     \
      _code                          \
   }
#define IOVEC_ARRAY_FIELD(_name)              \
   rpc->_name##_recv.iov_base = (void *) buf; \
   rpc->_name##_recv.iov_len = buflen;        \
   rpc->_name = &rpc->_name##_recv;           \
   rpc->n_##_name = 1;                        \
   buf = NULL;                                \
   buflen = 0;
#define SECTION_ARRAY_FIELD(_name)                                          \
   do {                                                                     \
      uint32_t __l;                                                         \
      mongoc_rpc_section_t *section = &rpc->_name[rpc->n_##_name];          \
      section->payload_type = buf[0];                                       \
      buf++;                                                                \
      buflen -= 1;                                                          \
      memcpy (&__l, buf, 4);                                                \
      __l = BSON_UINT32_FROM_LE (__l);                                      \
      if (section->payload_type == 0) {                                     \
         section->payload.bson_document = buf;                              \
      } else {                                                              \
         const uint8_t *section_buf = buf + 4;                              \
         section->payload.sequence.size = __l;                              \
         section->payload.sequence.identifier = (const char *) section_buf; \
         section_buf += strlen ((const char *) section_buf) + 1;            \
         section->payload.sequence.bson_documents = section_buf;            \
      }                                                                     \
      buf += __l;                                                           \
      buflen -= __l;                                                        \
      rpc->n_##_name++;                                                     \
   } while (buflen);
#define RAW_BUFFER_FIELD(_name)         \
   rpc->_name = (void *) buf;           \
   rpc->_name##_len = (int32_t) buflen; \
   buf = NULL;                          \
   buflen = 0;


#include "op-delete.def"
#include "op-get-more.def"
#include "op-header.def"
#include "op-insert.def"
#include "op-kill-cursors.def"
#include "op-msg.def"
#include "op-query.def"
#include "op-reply.def"
#include "op-reply-header.def"
#include "op-compressed.def"
#include "op-update.def"


#undef RPC
#undef ENUM_FIELD
#undef UINT8_FIELD
#undef INT32_FIELD
#undef INT64_FIELD
#undef INT64_ARRAY_FIELD
#undef CSTRING_FIELD
#undef BSON_FIELD
#undef BSON_ARRAY_FIELD
#undef IOVEC_ARRAY_FIELD
#undef SECTION_ARRAY_FIELD
#undef BSON_OPTIONAL
#undef RAW_BUFFER_FIELD


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_gather --
 *
 *       Takes a (native endian) rpc struct and gathers the buffer.
 *       Caller should swab to little endian after calling gather.
 *
 *       Gather, swab, compress write.
 *       Read, scatter, uncompress, swab
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_rpc_gather (mongoc_rpc_t *rpc, mongoc_array_t *array)
{
   mongoc_counter_op_egress_total_inc ();
   switch ((mongoc_opcode_t) rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_gather_reply (&rpc->reply, &rpc->header, array);
      return;

   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_gather_msg (&rpc->msg, &rpc->header, array);
      mongoc_counter_op_egress_msg_inc ();
      return;

   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_gather_update (&rpc->update, &rpc->header, array);
      mongoc_counter_op_egress_update_inc ();
      return;

   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_gather_insert (&rpc->insert, &rpc->header, array);
      mongoc_counter_op_egress_insert_inc ();
      return;

   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_gather_query (&rpc->query, &rpc->header, array);
      mongoc_counter_op_egress_query_inc ();
      return;

   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_gather_get_more (&rpc->get_more, &rpc->header, array);
      mongoc_counter_op_egress_getmore_inc ();
      return;

   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_gather_delete (&rpc->delete_, &rpc->header, array);
      mongoc_counter_op_egress_delete_inc ();
      return;

   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_gather_kill_cursors (&rpc->kill_cursors, &rpc->header, array);
      mongoc_counter_op_egress_killcursors_inc ();
      return;

   case MONGOC_OPCODE_COMPRESSED:
      _mongoc_rpc_gather_compressed (&rpc->compressed, &rpc->header, array);
      mongoc_counter_op_egress_compressed_inc ();
      return;

   default:
      MONGOC_WARNING ("Unknown rpc type: 0x%08x", rpc->header.opcode);
      BSON_ASSERT (false);
      break;
   }
}


void
_mongoc_rpc_swab_to_le (mongoc_rpc_t *rpc)
{
   BSON_UNUSED (rpc);

#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   {
      mongoc_opcode_t opcode;

      opcode = rpc->header.opcode;

      switch (opcode) {
      case MONGOC_OPCODE_REPLY:
         _mongoc_rpc_swab_to_le_reply (&rpc->reply);
         break;
      case MONGOC_OPCODE_MSG:
         _mongoc_rpc_swab_to_le_msg (&rpc->msg);
         break;
      case MONGOC_OPCODE_UPDATE:
         _mongoc_rpc_swab_to_le_update (&rpc->update);
         break;
      case MONGOC_OPCODE_INSERT:
         _mongoc_rpc_swab_to_le_insert (&rpc->insert);
         break;
      case MONGOC_OPCODE_QUERY:
         _mongoc_rpc_swab_to_le_query (&rpc->query);
         break;
      case MONGOC_OPCODE_GET_MORE:
         _mongoc_rpc_swab_to_le_get_more (&rpc->get_more);
         break;
      case MONGOC_OPCODE_DELETE:
         _mongoc_rpc_swab_to_le_delete (&rpc->delete_);
         break;
      case MONGOC_OPCODE_KILL_CURSORS:
         _mongoc_rpc_swab_to_le_kill_cursors (&rpc->kill_cursors);
         break;
      case MONGOC_OPCODE_COMPRESSED:
         _mongoc_rpc_swab_to_le_compressed (&rpc->compressed);
         break;
      default:
         MONGOC_WARNING ("Unknown rpc type: 0x%08x", opcode);
         break;
      }
   }
#endif
#if 0
   _mongoc_rpc_printf (rpc);
#endif
}


void
_mongoc_rpc_swab_from_le (mongoc_rpc_t *rpc)
{
   BSON_UNUSED (rpc);

#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
   {
      mongoc_opcode_t opcode;

      opcode = BSON_UINT32_FROM_LE (rpc->header.opcode);

      switch (opcode) {
      case MONGOC_OPCODE_REPLY:
         _mongoc_rpc_swab_from_le_reply (&rpc->reply);
         break;
      case MONGOC_OPCODE_MSG:
         _mongoc_rpc_swab_from_le_msg (&rpc->msg);
         break;
      case MONGOC_OPCODE_UPDATE:
         _mongoc_rpc_swab_from_le_update (&rpc->update);
         break;
      case MONGOC_OPCODE_INSERT:
         _mongoc_rpc_swab_from_le_insert (&rpc->insert);
         break;
      case MONGOC_OPCODE_QUERY:
         _mongoc_rpc_swab_from_le_query (&rpc->query);
         break;
      case MONGOC_OPCODE_GET_MORE:
         _mongoc_rpc_swab_from_le_get_more (&rpc->get_more);
         break;
      case MONGOC_OPCODE_DELETE:
         _mongoc_rpc_swab_from_le_delete (&rpc->delete_);
         break;
      case MONGOC_OPCODE_KILL_CURSORS:
         _mongoc_rpc_swab_from_le_kill_cursors (&rpc->kill_cursors);
         break;
      case MONGOC_OPCODE_COMPRESSED:
         _mongoc_rpc_swab_from_le_compressed (&rpc->compressed);
         break;
      default:
         MONGOC_WARNING ("Unknown rpc type: 0x%08x", rpc->header.opcode);
         break;
      }
   }
#endif
#if 0
   _mongoc_rpc_printf (rpc);
#endif
}


void
_mongoc_rpc_printf (mongoc_rpc_t *rpc)
{
   switch ((mongoc_opcode_t) rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_printf_reply (&rpc->reply);
      break;
   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_printf_msg (&rpc->msg);
      break;
   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_printf_update (&rpc->update);
      break;
   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_printf_insert (&rpc->insert);
      break;
   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_printf_query (&rpc->query);
      break;
   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_printf_get_more (&rpc->get_more);
      break;
   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_printf_delete (&rpc->delete_);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_printf_kill_cursors (&rpc->kill_cursors);
      break;
   case MONGOC_OPCODE_COMPRESSED:
      _mongoc_rpc_printf_compressed (&rpc->compressed);
      break;
   default:
      MONGOC_WARNING ("Unknown rpc type: 0x%08x", rpc->header.opcode);
      break;
   }
   printf ("\n");
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_decompress --
 *
 *       Takes a (little endian) rpc struct assumed to be OP_COMPRESSED
 *       and decompresses the opcode into its original opcode.
 *       The in-place updated rpc struct remains little endian.
 *
 * Side effects:
 *       Overwrites the RPC, along with the provided buf with the
 *       compressed results.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_rpc_decompress (mongoc_rpc_t *rpc_le, uint8_t *buf, size_t buflen)
{
   size_t uncompressed_size =
      BSON_UINT32_FROM_LE (rpc_le->compressed.uncompressed_size);
   bool ok;
   size_t msg_len = BSON_UINT32_TO_LE (buflen);
   const size_t original_uncompressed_size = uncompressed_size;

   BSON_ASSERT (uncompressed_size <= buflen);
   memcpy (buf, (void *) (&msg_len), 4);
   memcpy (buf + 4, (void *) (&rpc_le->header.request_id), 4);
   memcpy (buf + 8, (void *) (&rpc_le->header.response_to), 4);
   memcpy (buf + 12, (void *) (&rpc_le->compressed.original_opcode), 4);

   ok = mongoc_uncompress (rpc_le->compressed.compressor_id,
                           rpc_le->compressed.compressed_message,
                           rpc_le->compressed.compressed_message_len,
                           buf + 16,
                           &uncompressed_size);

   BSON_ASSERT (original_uncompressed_size == uncompressed_size);

   if (ok) {
      return _mongoc_rpc_scatter (rpc_le, buf, buflen);
   }

   return false;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_compress --
 *
 *       Takes a (little endian) rpc struct and creates a OP_COMPRESSED
 *       compressed opcode based on the provided compressor_id.
 *       The in-place updated rpc struct remains little endian.
 *
 * Side effects:
 *       Overwrites the RPC, and clears and overwrites the cluster buffer
 *       with the compressed results.
 *
 *--------------------------------------------------------------------------
 */

char *
_mongoc_rpc_compress (struct _mongoc_cluster_t *cluster,
                      int32_t compressor_id,
                      mongoc_rpc_t *rpc_le,
                      bson_error_t *error)
{
   char *output;
   size_t output_length = 0;
   size_t allocate = BSON_UINT32_FROM_LE (rpc_le->header.msg_len) - 16;
   char *data;
   int size;
   int32_t compression_level = -1;

   if (compressor_id == MONGOC_COMPRESSOR_ZLIB_ID) {
      compression_level = mongoc_uri_get_option_as_int32 (
         cluster->uri, MONGOC_URI_ZLIBCOMPRESSIONLEVEL, -1);
   }

   BSON_ASSERT (allocate > 0);
   data = bson_malloc0 (allocate);
   size = _mongoc_cluster_buffer_iovec (
      cluster->iov.data, cluster->iov.len, 16, data);
   BSON_ASSERT (size);

   output_length =
      mongoc_compressor_max_compressed_length (compressor_id, size);
   if (!output_length) {
      bson_set_error (error,
                      MONGOC_ERROR_COMMAND,
                      MONGOC_ERROR_COMMAND_INVALID_ARG,
                      "Could not determine compression bounds for %s",
                      mongoc_compressor_id_to_name (compressor_id));
      bson_free (data);
      return NULL;
   }

   output = (char *) bson_malloc0 (output_length);
   if (mongoc_compress (compressor_id,
                        compression_level,
                        data,
                        size,
                        output,
                        &output_length)) {
      rpc_le->header.msg_len = 0;
      rpc_le->compressed.original_opcode =
         BSON_UINT32_FROM_LE (rpc_le->header.opcode);
      rpc_le->header.opcode = MONGOC_OPCODE_COMPRESSED;
      rpc_le->header.request_id =
         BSON_UINT32_FROM_LE (rpc_le->header.request_id);
      rpc_le->header.response_to =
         BSON_UINT32_FROM_LE (rpc_le->header.response_to);

      rpc_le->compressed.uncompressed_size = size;
      rpc_le->compressed.compressor_id = compressor_id;
      rpc_le->compressed.compressed_message = (const uint8_t *) output;
      rpc_le->compressed.compressed_message_len = output_length;
      bson_free (data);


      _mongoc_array_destroy (&cluster->iov);
      _mongoc_array_init (&cluster->iov, sizeof (mongoc_iovec_t));
      _mongoc_rpc_gather (rpc_le, &cluster->iov);
      _mongoc_rpc_swab_to_le (rpc_le);
      return output;
   } else {
      MONGOC_WARNING ("Could not compress data with %s",
                      mongoc_compressor_id_to_name (compressor_id));
   }
   bson_free (data);
   bson_free (output);
   return NULL;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_scatter --
 *
 *       Takes a (little endian) rpc struct and scatters the buffer.
 *       Caller should check if resulting opcode is OP_COMPRESSED
 *       BEFORE swabbing to native endianness.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_rpc_scatter (mongoc_rpc_t *rpc, const uint8_t *buf, size_t buflen)
{
   mongoc_opcode_t opcode;

   memset (rpc, 0, sizeof *rpc);

   if (BSON_UNLIKELY (buflen < 16)) {
      return false;
   }

   mongoc_counter_op_ingress_total_inc ();
   if (!_mongoc_rpc_scatter_header (&rpc->header, buf, 16)) {
      return false;
   }

   opcode = (mongoc_opcode_t) BSON_UINT32_FROM_LE (rpc->header.opcode);

   switch (opcode) {
   case MONGOC_OPCODE_COMPRESSED:
      mongoc_counter_op_ingress_compressed_inc ();
      return _mongoc_rpc_scatter_compressed (&rpc->compressed, buf, buflen);

   case MONGOC_OPCODE_REPLY:
      mongoc_counter_op_ingress_reply_inc ();
      return _mongoc_rpc_scatter_reply (&rpc->reply, buf, buflen);

   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_ingress_msg_inc ();
      return _mongoc_rpc_scatter_msg (&rpc->msg, buf, buflen);


   /* useless, we are never *getting* these opcodes */
   case MONGOC_OPCODE_UPDATE:
      return _mongoc_rpc_scatter_update (&rpc->update, buf, buflen);

   case MONGOC_OPCODE_INSERT:
      return _mongoc_rpc_scatter_insert (&rpc->insert, buf, buflen);

   case MONGOC_OPCODE_QUERY:
      return _mongoc_rpc_scatter_query (&rpc->query, buf, buflen);

   case MONGOC_OPCODE_GET_MORE:
      return _mongoc_rpc_scatter_get_more (&rpc->get_more, buf, buflen);

   case MONGOC_OPCODE_DELETE:
      return _mongoc_rpc_scatter_delete (&rpc->delete_, buf, buflen);

   case MONGOC_OPCODE_KILL_CURSORS:
      return _mongoc_rpc_scatter_kill_cursors (&rpc->kill_cursors, buf, buflen);

   default:
      MONGOC_WARNING ("Unknown rpc type: 0x%08x", opcode);
      return false;
   }
}


bool
_mongoc_rpc_scatter_reply_header_only (mongoc_rpc_t *rpc,
                                       const uint8_t *buf,
                                       size_t buflen)
{
   if (BSON_UNLIKELY (buflen < sizeof (mongoc_rpc_reply_header_t))) {
      return false;
   }
   mongoc_counter_op_ingress_reply_inc ();
   mongoc_counter_op_ingress_total_inc ();
   return _mongoc_rpc_scatter_reply_header (&rpc->reply_header, buf, buflen);
}

bool
_mongoc_rpc_get_first_document (mongoc_rpc_t *rpc, bson_t *reply)
{
   if (rpc->header.opcode == MONGOC_OPCODE_MSG) {
      return _mongoc_rpc_reply_get_first_msg (&rpc->msg, reply);
   }

   if (rpc->header.opcode == MONGOC_OPCODE_REPLY &&
       _mongoc_rpc_reply_get_first (&rpc->reply, reply)) {
      return true;
   }

   return false;
}

/* Get the first BSON document from an OP_MSG reply: */
bool
_mongoc_rpc_reply_get_first_msg (mongoc_rpc_msg_t *reply_msg,
                                 bson_t *bson_reply)
{
   /* Note that mongo_rpc_reply_t is a union, with a mongo_rpc_msg_t field; see
   the *.def files and MongoDB Wire Protocol documentation for details. */

   int32_t document_len;

   BSON_ASSERT (0 == reply_msg->sections[0].payload_type);

   /* As per the Wire Protocol documentation, each section has a 32 bit length
   field: */
   memcpy (&document_len, reply_msg->sections[0].payload.bson_document, 4);
   document_len = BSON_UINT32_FROM_LE (document_len);

   bson_init_static (
      bson_reply, reply_msg->sections[0].payload.bson_document, document_len);

   return true;
}

bool
_mongoc_rpc_reply_get_first (mongoc_rpc_reply_t *reply, bson_t *bson)
{
   int32_t len;

   if (!reply->documents || reply->documents_len < 4) {
      return false;
   }

   memcpy (&len, reply->documents, 4);
   len = BSON_UINT32_FROM_LE (len);
   if (reply->documents_len < len) {
      return false;
   }

   return bson_init_static (bson, reply->documents, len);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_prep_command --
 *
 *       Prepare an RPC for mongoc_cluster_run_command_rpc. @cmd_ns and
 *       @cmd must not be freed or modified while the RPC is in use.
 *
 * Side effects:
 *       Fills out the RPC, including pointers into @cmd_ns and @command.
 *
 *--------------------------------------------------------------------------
 */

void
_mongoc_rpc_prep_command (mongoc_rpc_t *rpc,
                          const char *cmd_ns,
                          mongoc_cmd_t *cmd)

{
   rpc->header.msg_len = 0;
   rpc->header.request_id = 0;
   rpc->header.response_to = 0;
   rpc->header.opcode = MONGOC_OPCODE_QUERY;
   rpc->query.collection = cmd_ns;
   rpc->query.skip = 0;
   rpc->query.n_return = -1;
   rpc->query.fields = NULL;
   rpc->query.query = bson_get_data (cmd->command);

   /* Find, getMore And killCursors Commands Spec: "When sending a find command
    * rather than a legacy OP_QUERY find, only the secondaryOk flag is honored."
    * For other cursor-typed commands like aggregate, only secondaryOk can be
    * set. Clear bits except secondaryOk; leave secondaryOk set only if it is
    * already.
    */
   rpc->query.flags = cmd->query_flags & MONGOC_QUERY_SECONDARY_OK;
}


/* returns true if an error was found. */
static bool
_parse_error_reply (const bson_t *doc,
                    bool check_wce,
                    uint32_t *code,
                    const char **msg)
{
   bson_iter_t iter;
   bool found_error = false;

   ENTRY;

   BSON_ASSERT (doc);
   BSON_ASSERT (code);
   *code = 0;

   /* The server only returns real error codes as int32.
    * But it may return as a double or int64 if a failpoint
    * based on how it is configured to error. */
   if (bson_iter_init_find (&iter, doc, "code") &&
       BSON_ITER_HOLDS_NUMBER (&iter)) {
      *code = (uint32_t) bson_iter_as_int64 (&iter);
      BSON_ASSERT (*code);
      found_error = true;
   }

   if (bson_iter_init_find (&iter, doc, "errmsg") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      *msg = bson_iter_utf8 (&iter, NULL);
      found_error = true;
   } else if (bson_iter_init_find (&iter, doc, "$err") &&
              BSON_ITER_HOLDS_UTF8 (&iter)) {
      *msg = bson_iter_utf8 (&iter, NULL);
      found_error = true;
   }

   if (found_error) {
      /* there was a command error */
      RETURN (true);
   }

   if (check_wce) {
      /* check for a write concern error */
      if (bson_iter_init_find (&iter, doc, "writeConcernError") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_iter_t child;
         BSON_ASSERT (bson_iter_recurse (&iter, &child));
         if (bson_iter_find (&child, "code") &&
             BSON_ITER_HOLDS_NUMBER (&child)) {
            *code = (uint32_t) bson_iter_as_int64 (&child);
            BSON_ASSERT (*code);
            found_error = true;
         }
         BSON_ASSERT (bson_iter_recurse (&iter, &child));
         if (bson_iter_find (&child, "errmsg") &&
             BSON_ITER_HOLDS_UTF8 (&child)) {
            *msg = bson_iter_utf8 (&child, NULL);
            found_error = true;
         }
      }
   }

   RETURN (found_error);
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cmd_check_ok --
 *
 *       Check if a server reply document is an error message.
 *       Optionally fill out a bson_error_t from the server error.
 *       Does *not* check for writeConcernError.
 *
 * Returns:
 *       false if @doc is an error message, true otherwise.
 *
 * Side effects:
 *       If @doc is an error reply and @error is not NULL, set its
 *       domain, code, and message.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cmd_check_ok (const bson_t *doc,
                      int32_t error_api_version,
                      bson_error_t *error)
{
   mongoc_error_domain_t domain =
      error_api_version >= MONGOC_ERROR_API_VERSION_2 ? MONGOC_ERROR_SERVER
                                                      : MONGOC_ERROR_QUERY;
   uint32_t code;
   bson_iter_t iter;
   const char *msg = "Unknown command error";

   ENTRY;

   BSON_ASSERT (doc);

   if (bson_iter_init_find (&iter, doc, "ok") && bson_iter_as_bool (&iter)) {
      /* no error */
      RETURN (true);
   }

   if (!_parse_error_reply (doc, false /* check_wce */, &code, &msg)) {
      RETURN (true);
   }

   if (code == MONGOC_ERROR_PROTOCOL_ERROR || code == 13390) {
      code = MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND;
   } else if (code == 0) {
      code = MONGOC_ERROR_QUERY_FAILURE;
   }

   bson_set_error (error, domain, code, "%s", msg);

   /* there was a command error */
   RETURN (false);
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_cmd_check_ok_no_wce --
 *
 *       Check if a server reply document is an error message.
 *       Optionally fill out a bson_error_t from the server error.
 *       If the response contains a writeConcernError, this is considered
 *       an error and returns false.
 *
 * Returns:
 *       false if @doc is an error message, true otherwise.
 *
 * Side effects:
 *       If @doc is an error reply and @error is not NULL, set its
 *       domain, code, and message.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cmd_check_ok_no_wce (const bson_t *doc,
                             int32_t error_api_version,
                             bson_error_t *error)
{
   mongoc_error_domain_t domain =
      error_api_version >= MONGOC_ERROR_API_VERSION_2 ? MONGOC_ERROR_SERVER
                                                      : MONGOC_ERROR_QUERY;
   uint32_t code;
   const char *msg = "Unknown command error";

   ENTRY;

   BSON_ASSERT (doc);

   if (!_parse_error_reply (doc, true /* check_wce */, &code, &msg)) {
      RETURN (true);
   }

   if (code == MONGOC_ERROR_PROTOCOL_ERROR || code == 13390) {
      code = MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND;
   } else if (code == 0) {
      code = MONGOC_ERROR_QUERY_FAILURE;
   }

   bson_set_error (error, domain, code, "%s", msg);

   /* there was a command error */
   RETURN (false);
}


/* helper function to parse error reply document to an OP_QUERY */
static void
_mongoc_populate_query_error (const bson_t *doc,
                              int32_t error_api_version,
                              bson_error_t *error)
{
   mongoc_error_domain_t domain =
      error_api_version >= MONGOC_ERROR_API_VERSION_2 ? MONGOC_ERROR_SERVER
                                                      : MONGOC_ERROR_QUERY;
   uint32_t code = MONGOC_ERROR_QUERY_FAILURE;
   bson_iter_t iter;
   const char *msg = "Unknown query failure";

   ENTRY;

   BSON_ASSERT (doc);

   if (bson_iter_init_find (&iter, doc, "code") &&
       BSON_ITER_HOLDS_NUMBER (&iter)) {
      code = (uint32_t) bson_iter_as_int64 (&iter);
      BSON_ASSERT (code);
   }

   if (bson_iter_init_find (&iter, doc, "$err") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      msg = bson_iter_utf8 (&iter, NULL);
   }

   bson_set_error (error, domain, code, "%s", msg);

   EXIT;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_rpc_check_ok --
 *
 *       Check if a server OP_REPLY is an error message.
 *       Optionally fill out a bson_error_t from the server error.
 *       @error_document must be an initialized bson_t or NULL.
 *       Does *not* check for writeConcernError.
 *
 * Returns:
 *       false if the reply is an error message, true otherwise.
 *
 * Side effects:
 *       If rpc is an error reply and @error is not NULL, set its
 *       domain, code, and message.
 *
 *       If rpc is an error reply and @error_document is not NULL,
 *       it is reinitialized with the server reply.
 *
 *--------------------------------------------------------------------------
 */

bool
_mongoc_rpc_check_ok (mongoc_rpc_t *rpc,
                      int32_t error_api_version,
                      bson_error_t *error /* OUT */,
                      bson_t *error_doc /* OUT */)
{
   bson_t b;

   ENTRY;

   BSON_ASSERT (rpc);

   if (rpc->header.opcode != MONGOC_OPCODE_REPLY) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Received rpc other than OP_REPLY.");
      RETURN (false);
   }

   if (rpc->reply.flags & MONGOC_REPLY_QUERY_FAILURE) {
      if (_mongoc_rpc_get_first_document (rpc, &b)) {
         _mongoc_populate_query_error (&b, error_api_version, error);

         if (error_doc) {
            bson_destroy (error_doc);
            bson_copy_to (&b, error_doc);
         }

         bson_destroy (&b);
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_QUERY,
                         MONGOC_ERROR_QUERY_FAILURE,
                         "Unknown query failure.");
      }

      RETURN (false);
   } else if (rpc->reply.flags & MONGOC_REPLY_CURSOR_NOT_FOUND) {
      bson_set_error (error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "The cursor is invalid or has expired.");

      RETURN (false);
   }


   RETURN (true);
}

/* If rpc is OP_COMPRESSED, decompress it into buffer.
 *
 * Assumes rpc is still in network little-endian representation (i.e.
 * _mongoc_rpc_swab_to_le has not been called).
 * Returns true if rpc is not OP_COMPRESSED (and is a no-op) or if decompression
 * succeeds.
 * Return false and sets error otherwise.
 */
bool
_mongoc_rpc_decompress_if_necessary (mongoc_rpc_t *rpc,
                                     mongoc_buffer_t *buffer /* IN/OUT */,
                                     bson_error_t *error /* OUT */)
{
   uint8_t *buf = NULL;
   size_t len;

   if (BSON_UINT32_FROM_LE (rpc->header.opcode) != MONGOC_OPCODE_COMPRESSED) {
      return true;
   }

   len = BSON_UINT32_FROM_LE (rpc->compressed.uncompressed_size) +
         sizeof (mongoc_rpc_header_t);

   buf = bson_malloc0 (len);
   if (!_mongoc_rpc_decompress (rpc, buf, len)) {
      bson_free (buf);
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Could not decompress server reply");
      return false;
   }

   _mongoc_buffer_destroy (buffer);
   _mongoc_buffer_init (buffer, buf, len, NULL, NULL);

   return true;
}
