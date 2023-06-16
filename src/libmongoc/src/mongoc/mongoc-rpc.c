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


#include "mongoc-rpc-private.h"

#include "mongoc-counters-private.h"
#include "mongoc-trace-private.h"


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
#define CHECKSUM_FIELD(_name) // Do not include optional checksum.
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
#undef CHECKSUM_FIELD


#if BSON_BYTE_ORDER == BSON_BIG_ENDIAN

#define RPC(_name, _code)                                                   \
   static void _mongoc_rpc_swab_to_le_##_name (mongoc_rpc_##_name##_t *rpc) \
   {                                                                        \
      BSON_ASSERT (rpc);                                                    \
      _code                                                                 \
   }
#define UINT8_FIELD(_name)
#define INT32_FIELD(_name) rpc->_name = BSON_UINT32_FROM_LE (rpc->_name);
#define CHECKSUM_FIELD(_name) rpc->_name = BSON_UINT32_FROM_LE (rpc->_name);
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
#undef CHECKSUM_FIELD

#endif /* BSON_BYTE_ORDER == BSON_BIG_ENDIAN */


#define RPC(_name, _code)                                               \
   static void _mongoc_rpc_printf_##_name (mongoc_rpc_##_name##_t *rpc) \
   {                                                                    \
      BSON_ASSERT (rpc);                                                \
      _code                                                             \
   }
#define UINT8_FIELD(_name) printf ("  " #_name " : %u\n", rpc->_name);
#define INT32_FIELD(_name) printf ("  " #_name " : %" PRId32 "\n", rpc->_name);
#define CHECKSUM_FIELD(_name) \
   printf ("  " #_name " : %" PRIu32 "\n", rpc->_name);
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
#define SECTION_ARRAY_FIELD(_name)                                         \
   do {                                                                    \
      printf ("  " #_name " : %d\n", rpc->n_##_name);                      \
      for (ssize_t _i = 0; _i < rpc->n_##_name; _i++) {                    \
         if (rpc->_name[_i].payload_type == 0) {                           \
            do {                                                           \
               bson_t b;                                                   \
               char *s;                                                    \
               int32_t __l;                                                \
               memcpy (&__l, rpc->_name[_i].payload.bson_document, 4);     \
               __l = BSON_UINT32_FROM_LE (__l);                            \
               BSON_ASSERT (bson_init_static (                             \
                  &b, rpc->_name[_i].payload.bson_document, __l));         \
               s = bson_as_relaxed_extended_json (&b, NULL);               \
               printf ("  Type %d: %s\n", rpc->_name[_i].payload_type, s); \
               bson_free (s);                                              \
               bson_destroy (&b);                                          \
            } while (0);                                                   \
         } else if (rpc->_name[_i].payload_type == 1) {                    \
            bson_reader_t *__r;                                            \
            BSON_ASSERT (bson_in_range_signed (                            \
               size_t, rpc->_name[_i].payload.sequence.size));             \
            const size_t max_size =                                        \
               (size_t) rpc->_name[_i].payload.sequence.size -             \
               strlen (rpc->_name[_i].payload.sequence.identifier) - 1u -  \
               sizeof (int32_t);                                           \
            bool __eof;                                                    \
            const bson_t *__b;                                             \
            printf ("  Identifier: %s\n",                                  \
                    rpc->_name[_i].payload.sequence.identifier);           \
            printf ("  Size: %zu\n", max_size);                            \
            __r = bson_reader_new_from_data (                              \
               rpc->_name[_i].payload.sequence.bson_documents, max_size);  \
            while ((__b = bson_reader_read (__r, &__eof))) {               \
               char *s = bson_as_relaxed_extended_json (__b, NULL);        \
               bson_free (s);                                              \
            }                                                              \
            bson_reader_destroy (__r);                                     \
         }                                                                 \
      }                                                                    \
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
#undef CHECKSUM_FIELD


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
#define CHECKSUM_FIELD(_name)       \
   if (buflen >= 4) {               \
      memcpy (&rpc->_name, buf, 4); \
      buflen -= 4;                  \
      buf += 4;                     \
   }
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
   } while (buflen > 4); // Only optional checksum can come after data sections.
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
#undef CHECKSUM_FIELD


void
_mongoc_rpc_gather (mongoc_rpc_t *rpc, mongoc_array_t *array)
{
   switch ((mongoc_opcode_t) rpc->header.opcode) {
   case MONGOC_OPCODE_REPLY:
      _mongoc_rpc_gather_reply (&rpc->reply, &rpc->header, array);
      return;

   case MONGOC_OPCODE_MSG:
      _mongoc_rpc_gather_msg (&rpc->msg, &rpc->header, array);
      return;

   case MONGOC_OPCODE_UPDATE:
      _mongoc_rpc_gather_update (&rpc->update, &rpc->header, array);
      return;

   case MONGOC_OPCODE_INSERT:
      _mongoc_rpc_gather_insert (&rpc->insert, &rpc->header, array);
      return;

   case MONGOC_OPCODE_QUERY:
      _mongoc_rpc_gather_query (&rpc->query, &rpc->header, array);
      return;

   case MONGOC_OPCODE_GET_MORE:
      _mongoc_rpc_gather_get_more (&rpc->get_more, &rpc->header, array);
      return;

   case MONGOC_OPCODE_DELETE:
      _mongoc_rpc_gather_delete (&rpc->delete_, &rpc->header, array);
      return;

   case MONGOC_OPCODE_KILL_CURSORS:
      _mongoc_rpc_gather_kill_cursors (&rpc->kill_cursors, &rpc->header, array);
      return;

   case MONGOC_OPCODE_COMPRESSED:
      _mongoc_rpc_gather_compressed (&rpc->compressed, &rpc->header, array);
      return;

   default:
      MONGOC_WARNING ("Unknown rpc type: 0x%08x", rpc->header.opcode);
      BSON_ASSERT (false);
      break;
   }
}


void
_mongoc_rpc_op_egress_inc (const mongoc_rpc_t *rpc)
{
   mongoc_opcode_t opcode =
      (mongoc_opcode_t) BSON_UINT32_FROM_LE (rpc->header.opcode);

   if (opcode == MONGOC_OPCODE_COMPRESSED) {
      mongoc_counter_op_egress_compressed_inc ();
      mongoc_counter_op_egress_total_inc ();

      opcode = (mongoc_opcode_t) BSON_UINT32_FROM_LE (
         rpc->compressed.original_opcode);
   }

   mongoc_counter_op_egress_total_inc ();

   switch (opcode) {
   case MONGOC_OPCODE_REPLY:
      return;

   case MONGOC_OPCODE_MSG:
      mongoc_counter_op_egress_msg_inc ();
      return;

   case MONGOC_OPCODE_UPDATE:
      mongoc_counter_op_egress_update_inc ();
      return;

   case MONGOC_OPCODE_INSERT:
      mongoc_counter_op_egress_insert_inc ();
      return;

   case MONGOC_OPCODE_QUERY:
      mongoc_counter_op_egress_query_inc ();
      return;

   case MONGOC_OPCODE_GET_MORE:
      mongoc_counter_op_egress_getmore_inc ();
      return;

   case MONGOC_OPCODE_DELETE:
      mongoc_counter_op_egress_delete_inc ();
      return;

   case MONGOC_OPCODE_KILL_CURSORS:
      mongoc_counter_op_egress_killcursors_inc ();
      return;

   case MONGOC_OPCODE_COMPRESSED:
      MONGOC_WARNING ("Compressed an OP_COMPRESSED message!?");
      BSON_ASSERT (false);
      return;

   default:
      MONGOC_WARNING ("Unknown rpc type: 0x%08x", opcode);
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

   if (rpc->header.opcode == MONGOC_OPCODE_REPLY) {
      return _mongoc_rpc_reply_get_first (&rpc->reply, reply);
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

   if (BSON_UNLIKELY (reply_msg->sections[0].payload_type != 0)) {
      return false;
   }

   /* As per the Wire Protocol documentation, each section has a 32 bit length
   field: */
   memcpy (&document_len, reply_msg->sections[0].payload.bson_document, 4);
   document_len = BSON_UINT32_FROM_LE (document_len);

   return bson_init_static (
      bson_reply, reply_msg->sections[0].payload.bson_document, document_len);
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

bool
mcd_rpc_message_get_body (const mcd_rpc_message *rpc, bson_t *reply)
{
   switch (mcd_rpc_header_get_op_code (rpc)) {
   case MONGOC_OP_CODE_MSG: {
      const size_t sections_count = mcd_rpc_op_msg_get_sections_count (rpc);

      // Look for section kind 0.
      for (size_t index = 0u; index < sections_count; ++index) {
         switch (mcd_rpc_op_msg_section_get_kind (rpc, index)) {
         case 0: { // Body.
            const uint8_t *const body =
               mcd_rpc_op_msg_section_get_body (rpc, index);

            const int32_t body_len =
               bson_iter_int32_unsafe (&(bson_iter_t){.raw = body});

            return bson_init_static (reply, body, (size_t) body_len);
         }

         case 1: // Document Sequence.
            continue;

         default:
            // Validated by `mcd_rpc_message_from_data`.
            BSON_UNREACHABLE ("invalid OP_MSG section kind");
         }
      }
      break;
   }

   case MONGOC_OP_CODE_REPLY: {
      if (mcd_rpc_op_reply_get_documents_len (rpc) < 1) {
         return false;
      }

      // Assume the first document in OP_REPLY is the body.
      const uint8_t *const body = mcd_rpc_op_reply_get_documents (rpc);

      return bson_init_static (
         reply,
         body,
         (size_t) bson_iter_int32_unsafe (&(bson_iter_t){.raw = body}));
   }

   default:
      break;
   }

   return false;
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

bool
mcd_rpc_message_check_ok (mcd_rpc_message *rpc,
                          int32_t error_api_version,
                          bson_error_t *error /* OUT */,
                          bson_t *error_doc /* OUT */)
{
   BSON_ASSERT (rpc);

   ENTRY;

   if (mcd_rpc_header_get_op_code (rpc) != MONGOC_OP_CODE_REPLY) {
      bson_set_error (error,
                      MONGOC_ERROR_PROTOCOL,
                      MONGOC_ERROR_PROTOCOL_INVALID_REPLY,
                      "Received rpc other than OP_REPLY.");
      RETURN (false);
   }

   const int32_t flags = mcd_rpc_op_reply_get_response_flags (rpc);

   if (flags & MONGOC_OP_REPLY_RESPONSE_FLAG_QUERY_FAILURE) {
      bson_t body;

      if (mcd_rpc_message_get_body (rpc, &body)) {
         _mongoc_populate_query_error (&body, error_api_version, error);

         if (error_doc) {
            bson_destroy (error_doc);
            bson_copy_to (&body, error_doc);
         }

         bson_destroy (&body);
      } else {
         bson_set_error (error,
                         MONGOC_ERROR_QUERY,
                         MONGOC_ERROR_QUERY_FAILURE,
                         "Unknown query failure.");
      }

      RETURN (false);
   }

   if (flags & MONGOC_OP_REPLY_RESPONSE_FLAG_CURSOR_NOT_FOUND) {
      bson_set_error (error,
                      MONGOC_ERROR_CURSOR,
                      MONGOC_ERROR_CURSOR_INVALID_CURSOR,
                      "The cursor is invalid or has expired.");

      RETURN (false);
   }


   RETURN (true);
}

void
mcd_rpc_message_egress (const mcd_rpc_message *rpc)
{
   // `mcd_rpc_message_egress` is expected to be called after
   // `mcd_rpc_message_to_iovecs`, which converts the opCode field to
   // little endian.
   int32_t op_code = mcd_rpc_header_get_op_code (rpc);
   op_code = bson_iter_int32_unsafe (
      &(bson_iter_t){.raw = (const uint8_t *) &op_code});

   if (op_code == MONGOC_OP_CODE_COMPRESSED) {
      mongoc_counter_op_egress_compressed_inc ();
      mongoc_counter_op_egress_total_inc ();

      op_code = mcd_rpc_op_compressed_get_original_opcode (rpc);
      op_code = bson_iter_int32_unsafe (
         &(bson_iter_t){.raw = (const uint8_t *) &op_code});
   }

   switch (op_code) {
   case MONGOC_OP_CODE_COMPRESSED:
      BSON_UNREACHABLE ("invalid opcode (double compression?!)");
      break;

   case MONGOC_OP_CODE_MSG:
      mongoc_counter_op_egress_msg_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_REPLY:
      BSON_UNREACHABLE ("unexpected OP_REPLY egress");
      break;

   case MONGOC_OP_CODE_UPDATE:
      mongoc_counter_op_egress_update_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_INSERT:
      mongoc_counter_op_egress_insert_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_QUERY:
      mongoc_counter_op_egress_query_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_GET_MORE:
      mongoc_counter_op_egress_getmore_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_DELETE:
      mongoc_counter_op_egress_delete_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   case MONGOC_OP_CODE_KILL_CURSORS:
      mongoc_counter_op_egress_killcursors_inc ();
      mongoc_counter_op_egress_total_inc ();
      break;

   default:
      BSON_UNREACHABLE ("invalid opcode");
   }
}

void
mcd_rpc_message_ingress (const mcd_rpc_message *rpc)
{
   // `mcd_rpc_message_ingress` is expected be called after
   // `mcd_rpc_message_from_data`, which converts the opCode field to native
   // endian.
   int32_t op_code = mcd_rpc_header_get_op_code (rpc);

   if (op_code == MONGOC_OP_CODE_COMPRESSED) {
      mongoc_counter_op_ingress_compressed_inc ();
      mongoc_counter_op_ingress_total_inc ();

      op_code = mcd_rpc_op_compressed_get_original_opcode (rpc);
   }

   switch (op_code) {
   case MONGOC_OP_CODE_COMPRESSED:
      BSON_UNREACHABLE ("invalid opcode (double compression?!)");
      break;

   case MONGOC_OP_CODE_MSG:
      mongoc_counter_op_ingress_msg_inc ();
      mongoc_counter_op_ingress_total_inc ();
      break;

   case MONGOC_OP_CODE_REPLY:
      mongoc_counter_op_ingress_reply_inc ();
      mongoc_counter_op_ingress_total_inc ();
      break;

   case MONGOC_OP_CODE_UPDATE:
      BSON_UNREACHABLE ("unexpected OP_UPDATE ingress");
      break;

   case MONGOC_OP_CODE_INSERT:
      BSON_UNREACHABLE ("unexpected OP_INSERT ingress");
      break;

   case MONGOC_OP_CODE_QUERY:
      BSON_UNREACHABLE ("unexpected OP_QUERY ingress");
      break;

   case MONGOC_OP_CODE_GET_MORE:
      BSON_UNREACHABLE ("unexpected OP_GET_MORE ingress");
      break;

   case MONGOC_OP_CODE_DELETE:
      BSON_UNREACHABLE ("unexpected OP_DELETE ingress");
      break;

   case MONGOC_OP_CODE_KILL_CURSORS:
      BSON_UNREACHABLE ("unexpected OP_KILL_CURSORS ingress");
      break;

   default:
      BSON_UNREACHABLE ("invalid opcode");
   }
}
