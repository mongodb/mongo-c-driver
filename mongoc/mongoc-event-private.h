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


#ifndef MONGOC_EVENT_H
#define MONGOC_EVENT_H


#include <alloca.h>
#include <bson.h>
#include <sys/uio.h>

#include "mongoc-buffer-private.h"
#include "mongoc-flags.h"
#include "mongoc-stream.h"


BSON_BEGIN_DECLS


#define MONGOC_EVENT_INITIALIZER(t) {(mongoc_opcode_t)(t)}
#define MONGOC_EVENT_MAX_LEN        (1024 * 1024 * 48)


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
   mongoc_buffer_t rawbuf;

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

#pragma pack(push, 1)
   struct {
      bson_uint32_t     flags;
      bson_uint64_t     cursor_id;
      bson_uint32_t     start_from;
      bson_uint32_t     n_returned;
   } desc;
#pragma pack(pop)

   bson_reader_t        docs_reader;

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
mongoc_event_write (mongoc_event_t  *event,
                    mongoc_stream_t *stream,
                    bson_error_t    *error);


bson_bool_t
mongoc_event_read (mongoc_event_t  *event,
                   mongoc_stream_t *stream,
                   bson_error_t    *error);


#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
#define MONGOC_EVENT_SWAB_HEADER(e) \
   do { \
      (e)->any.len = BSON_UINT32_TO_LE((e)->any.len); \
      (e)->any.request_id = BSON_UINT32_TO_LE((e)->any.request_id); \
      (e)->any.response_to = BSON_UINT32_TO_LE((e)->any.response_to); \
      (e)->any.opcode = BSON_UINT32_TO_LE((e)->any.opcode); \
   } while (0)
#define MONGOC_EVENT_SWAB_DELETE(e) \
   do { \
      (e)->delete.flags = BSON_UINT32_TO_LE((e)->delete.flags); \
   } while (0)
#define MONGOC_EVENT_SWAB_GET_MORE(e) \
   do { \
      (e)->get_more.n_return = BSON_UINT32_TO_LE((e)->get_more.n_return); \
      (e)->get_more.cursor_id = BSON_UINT64_TO_LE((e)->get_more.cursor_id); \
   } while (0)
#define MONGOC_EVENT_SWAB_INSERT(e) \
   do { \
      (e)->insert.flags = BSON_UINT32_TO_LE((e)->insert.flags); \
   } while (0)
#define MONGOC_EVENT_SWAB_KILL_CURSORS(e) \
   do { \
      bson_uint32_t _i; \
      for (_i = 0; _i < (e)->kill_cursors.n_cursors; _i++) { \
         (e)->kill_cursors.cursors[_i] = BSON_UINT64_TO_LE((e)->kill_cursors.cursors[_i]); \
      } \
      (e)->kill_cursors.n_cursors = BSON_UINT32_TO_LE((e)->kill_cursors.n_cursors); \
   } while (0)
#define MONGOC_EVENT_SWAB_MSG(e)
#define MONGOC_EVENT_SWAB_QUERY(e) \
   do { \
      (e)->query.flags = BSON_UINT32_TO_LE((e)->query.flags); \
      (e)->query.skip = BSON_UINT32_TO_LE((e)->query.skip); \
      (e)->query.n_return = BSON_UINT32_TO_LE((e)->query.n_return); \
   } while (0)
#define MONGOC_EVENT_SWAB_REPLY(e) \
   do { \
      (e)->reply.desc.flags = BSON_UINT32_TO_LE((e)->reply.desc.flags); \
      (e)->reply.desc.cursor_id = BSON_UINT64_TO_LE((e)->reply.desc.cursor_id); \
      (e)->reply.desc.start_from = BSON_UINT32_TO_LE((e)->reply.desc.start_from); \
      (e)->reply.desc.n_returned = BSON_UINT32_TO_LE((e)->reply.desc.n_returned); \
   } while (0)
#define MONGOC_EVENT_SWAB_UPDATE(e) \
   do { \
      (e)->update.flags = BSON_UINT32_TO_LE((e)->update.flags); \
   } while (0)
#else
#define MONGOC_EVENT_SWAB_HEADER(e)
#define MONGOC_EVENT_SWAB_DELETE(e)
#define MONGOC_EVENT_SWAB_GET_MORE(e)
#define MONGOC_EVENT_SWAB_INSERT(e)
#define MONGOC_EVENT_SWAB_KILL_CURSORS(e)
#define MONGOC_EVENT_SWAB_MSG(e)
#define MONGOC_EVENT_SWAB_QUERY(e)
#define MONGOC_EVENT_SWAB_REPLY(e)
#define MONGOC_EVENT_SWAB_UPDATE(e)
#endif


#define MONGOC_EVENT_SCATTER_DELETE(e, iov, iovcnt) \
   do { \
      iovcnt = 5; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 25 + e->delete.nslen + e->delete.selector->len; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->delete.zero; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = (void *)e->delete.ns; \
      iov[2].iov_len = e->delete.nslen + 1; \
      iov[3].iov_base = &e->delete.flags; \
      iov[3].iov_len = 4; \
      iov[4].iov_base = (void *)bson_get_data(e->delete.selector); \
      iov[4].iov_len = e->delete.selector->len; \
   } while (0)


#define MONGOC_EVENT_SCATTER_GET_MORE(e, iov, iovcnt) \
   do { \
      iovcnt = 5; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 33 + e->get_more.nslen; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->get_more.zero; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = (void *)e->get_more.ns; \
      iov[2].iov_len = e->get_more.nslen + 1; \
      iov[3].iov_base = &e->get_more.n_return; \
      iov[3].iov_len = 4; \
      iov[4].iov_base = &e->get_more.cursor_id; \
      iov[4].iov_len = 8; \
   } while (0)


#define MONGOC_EVENT_SCATTER_KILL_CURSORS(e, iov, iovcnt) \
   do { \
      bson_uint32_t _i; \
      iovcnt = 4; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 24 + (8 * e->kill_cursors.n_cursors); \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->kill_cursors.zero; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = &e->kill_cursors.n_cursors; \
      iov[2].iov_len = 4; \
      iov[3].iov_base = (void *)e->kill_cursors.cursors; \
      iov[3].iov_len = 8 * e->kill_cursors.n_cursors; \
      for (_i = 0; _i < e->kill_cursors.n_cursors; _i++) { \
         e->kill_cursors.cursors[_i] = BSON_UINT64_TO_LE(e->kill_cursors.cursors[_i]); \
      } \
   } while (0)


#define MONGOC_EVENT_SCATTER_MSG(e, iov, iovcnt) \
   do { \
      iovcnt = 2; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      event->any.len = 17 + event->msg.msglen; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = (void *)event->msg.msg; \
      iov[1].iov_len = event->msg.msglen + 1; \
   } while (0)


#define MONGOC_EVENT_SCATTER_REPLY(e, iov, iovcnt) \
   do { \
      bson_uint32_t _i; \
      iovcnt = 2 + e->reply.docslen; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 36; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->reply.desc; \
      iov[1].iov_len = 20; \
      for (_i = 0; _i < e->reply.docslen; _i++) { \
         e->any.len += e->reply.docs[_i]->len; \
         iov[2 + _i].iov_len = e->reply.docs[_i]->len; \
         iov[2 + _i].iov_base = (void *)bson_get_data(e->reply.docs[_i]); \
      } \
   } while (0)


#define MONGOC_EVENT_SCATTER_QUERY(e, iov, iovcnt) \
   do { \
      iovcnt = 6; \
      iov = alloca(sizeof(struct iovec) * 7); \
      e->any.len = 29 + e->query.nslen + e->query.query->len; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->query.flags; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = (void *)e->query.ns; \
      iov[2].iov_len = e->query.nslen + 1; \
      iov[3].iov_base = &e->query.skip; \
      iov[3].iov_len = 4; \
      iov[4].iov_base = &e->query.n_return; \
      iov[4].iov_len = 4; \
      iov[5].iov_base = (void *)bson_get_data(e->query.query); \
      iov[5].iov_len = e->query.query->len; \
      if (e->query.fields) { \
         e->any.len += e->query.fields->len; \
         iov[6].iov_base = (void *)bson_get_data(e->query.fields); \
         iov[6].iov_len = e->query.fields->len; \
         iovcnt++; \
      } \
   } while (0)


#define MONGOC_EVENT_SCATTER_UPDATE(e, iov, iovcnt) \
   do { \
      iovcnt = 6; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 25 + e->update.nslen + e->update.selector->len + e->update.update->len; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->update.zero; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = (void *)e->update.ns; \
      iov[2].iov_len = e->update.nslen + 1; \
      iov[3].iov_base = &e->update.flags; \
      iov[3].iov_len = 4; \
      iov[4].iov_base = (void *)bson_get_data(e->update.selector); \
      iov[4].iov_len = e->update.selector->len; \
      iov[5].iov_base = (void *)bson_get_data(e->update.update); \
      iov[5].iov_len = e->update.update->len; \
   } while (0)


#define MONGOC_EVENT_SCATTER_INSERT(e, iov, iovcnt) \
   do { \
      bson_uint32_t _i; \
      iovcnt = 3 + e->insert.docslen; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 21 + e->insert.nslen; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->insert.flags; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = (void *)e->insert.ns; \
      iov[2].iov_len = e->insert.nslen + 1; \
      for (_i = 0; _i < e->insert.docslen; _i++) { \
         e->any.len += e->insert.docs[_i]->len; \
         iov[3+_i].iov_len = e->insert.docs[_i]->len; \
         iov[3+_i].iov_base = (void *)bson_get_data(e->insert.docs[_i]); \
      } \
   } while (0)


BSON_END_DECLS


#endif /* MONGOC_EVENT_H */
