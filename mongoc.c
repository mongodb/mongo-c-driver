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


#include <alloca.h>
#include <errno.h>
#include <sys/uio.h>

#include "mongoc.h"
#include "mongoc-uri.h"


#define MONGOC_EVENT_MAX_LEN (1024 * 1024 * 48)


#if BSON_BYTE_ORDER != BSON_LITTLE_ENDIAN
#define SWAB_HEADER(e)                                                \
   do {                                                               \
      (e)->any.len = BSON_UINT32_TO_LE((e)->any.len);                 \
      (e)->any.request_id = BSON_UINT32_TO_LE((e)->any.request_id);   \
      (e)->any.response_to = BSON_UINT32_TO_LE((e)->any.response_to); \
      (e)->any.opcode = BSON_UINT32_TO_LE((e)->any.opcode);           \
   } while (0)
#define SWAB_DELETE(e)                                          \
   do {                                                         \
      (e)->delete.flags = BSON_UINT32_TO_LE((e)->delete.flags); \
   } while (0)
#define SWAB_GET_MORE(e)                                                    \
   do {                                                                     \
      (e)->get_more.n_return = BSON_UINT32_TO_LE((e)->get_more.n_return);   \
      (e)->get_more.cursor_id = BSON_UINT64_TO_LE((e)->get_more.cursor_id); \
   } while (0)
#define SWAB_INSERT(e)                                          \
   do {                                                         \
      (e)->insert.flags = BSON_UINT32_TO_LE((e)->insert.flags); \
   } while (0)
#define SWAB_KILL_CURSORS(e)                                                               \
   do {                                                                                    \
      bson_uint32_t _i;                                                                    \
      for (_i = 0; _i < (e)->kill_cursors.n_cursors; _i++) {                               \
         (e)->kill_cursors.cursors[_i] = BSON_UINT64_TO_LE((e)->kill_cursors.cursors[_i]); \
      }                                                                                    \
      (e)->kill_cursors.n_cursors = BSON_UINT32_TO_LE((e)->kill_cursors.n_cursors);        \
   } while (0)
#define SWAB_MSG(e)
#define SWAB_QUERY(e)                                               \
   do {                                                             \
      (e)->query.flags = BSON_UINT32_TO_LE((e)->query.flags);       \
      (e)->query.skip = BSON_UINT32_TO_LE((e)->query.skip);         \
      (e)->query.n_return = BSON_UINT32_TO_LE((e)->query.n_return); \
   } while (0)
#define SWAB_REPLY(e)                                                   \
   do {                                                                 \
      (e)->reply.flags = BSON_UINT32_TO_LE((e)->reply.flags);           \
      (e)->reply.cursor_id = BSON_UINT64_TO_LE((e)->reply.cursor_id);   \
      (e)->reply.start_from = BSON_UINT32_TO_LE((e)->reply.start_from); \
      (e)->reply.n_returned = BSON_UINT32_TO_LE((e)->reply.n_returned); \
   } while (0)
#define SWAB_UPDATE(e)                                          \
   do {                                                         \
      (e)->update.flags = BSON_UINT32_TO_LE((e)->update.flags); \
   } while (0)
#else
#define SWAB_HEADER(e)
#define SWAB_DELETE(e)
#define SWAB_GET_MORE(e)
#define SWAB_INSERT(e)
#define SWAB_KILL_CURSORS(e)
#define SWAB_MSG(e)
#define SWAB_QUERY(e)
#define SWAB_REPLY(e)
#define SWAB_UPDATE(e)
#endif


#define SCATTER_DELETE(e, iov, iovcnt) \
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
      iov[4].iov_base = &e->delete.selector->top.data; \
      iov[4].iov_len = e->delete.selector->len; \
   } while (0)

#define SCATTER_GET_MORE(e, iov, iovcnt) \
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

#define SCATTER_KILL_CURSORS(e, iov, iovcnt) \
   do { \
      iovcnt = 4; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 24 + (8 * e->kill_cursors.n_cursors); \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->kill_cursors.zero; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = &e->kill_cursors.n_cursors; \
      iov[2].iov_len = 4; \
      iov[3].iov_base = &e->kill_cursors.cursors; \
      iov[3].iov_len = 8 * e->kill_cursors.n_cursors; \
   } while (0)

#define SCATTER_MSG(e, iov, iovcnt) \
   do { \
      iovcnt = 2; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      event->any.len = 17 + event->msg.msglen; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = (void *)event->msg.msg; \
      iov[1].iov_len = event->msg.msglen + 1; \
   } while (0)

#define SCATTER_REPLY(e, iov, iovcnt) \
   do { \
      bson_uint32_t _i; \
      iovcnt = 5 + e->reply.docslen; \
      iov = alloca(sizeof(struct iovec) * iovcnt); \
      e->any.len = 36; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->reply.flags; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = &e->reply.cursor_id; \
      iov[2].iov_len = 8; \
      iov[3].iov_base = &e->reply.start_from; \
      iov[3].iov_len = 4; \
      iov[4].iov_base = &e->reply.n_returned; \
      iov[4].iov_len = 4; \
      for (_i = 0; _i < e->reply.docslen; _i++) { \
         e->any.len += e->reply.docs[_i]->len; \
         iov[5 + _i].iov_len = e->reply.docs[_i]->len; \
         iov[5 + _i].iov_base = e->reply.docs[_i]->top.data; \
      } \
   } while (0)

#define SCATTER_QUERY(e, iov, iovcnt) \
   do { \
      iovcnt = 6; \
      iov = alloca(sizeof(struct iovec) * 7); \
      e->any.len = 29 + e->query.nslen + e->query.query->len; \
      iov[0].iov_base = &e->any.len; \
      iov[0].iov_len = 16; \
      iov[1].iov_base = &e->query.flags; \
      iov[1].iov_len = 4; \
      iov[2].iov_base = &e->query.ns; \
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

#define SCATTER_UPDATE(e, iov, iovcnt) \
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

#define SCATTER_INSERT(e, iov, iovcnt) \
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


struct _mongoc_client_t
{
   mongoc_uri_t  *uri;
   bson_uint32_t  request_id;
   int            outfd;
};


bson_bool_t
mongoc_event_encode (mongoc_event_t     *event,
                     bson_uint8_t      **buf,
                     size_t             *buflen,
                     bson_realloc_func   realloc_func,
                     bson_error_t       *error)
{
   bson_uint32_t len;
   bson_uint32_t i;
   bson_uint8_t *dst;
   struct iovec *iov;
   int iovcnt;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(buf, FALSE);
   bson_return_val_if_fail(buflen, FALSE);

   if (!realloc_func) {
      realloc_func = bson_realloc;
   }

   switch (event->type) {
   case MONGOC_OPCODE_QUERY:
      SCATTER_QUERY(event, iov, iovcnt);
      SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      SCATTER_UPDATE(event, iov, iovcnt);
      SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      SCATTER_INSERT(event, iov, iovcnt);
      SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      SCATTER_GET_MORE(event, iov, iovcnt);
      SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      SCATTER_DELETE(event, iov, iovcnt);
      SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      SCATTER_KILL_CURSORS(event, iov, iovcnt);
      SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      SCATTER_MSG(event, iov, iovcnt);
      SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      SCATTER_REPLY(event, iov, iovcnt);
      SWAB_REPLY(event);
      break;
   default:
      return FALSE;
   }

   len = event->any.len;

   if (len > MONGOC_EVENT_MAX_LEN) {
      bson_set_error(error,
                     1, /* MONGOC_CLIENT_ERROR, */
                     1, /* MONGOC_CLIENT_ERROR_MSG_TOO_LARGE, */
                     "The event length is too large: %u",
                     len);
      return FALSE;
   }

   SWAB_HEADER(event);

   if (!*buf || (*buflen < len)) {
      *buf = realloc_func(*buf, len);
   }

   dst = *buf;
   *buflen = len;

   for (i = 0; i < iovcnt; i++) {
      memcpy(dst, iov[i].iov_base, iov[i].iov_len);
      dst += iov[i].iov_len;
   }

   return TRUE;
}


bson_bool_t
mongoc_event_write (mongoc_event_t *event,
                    int             sd,
                    bson_error_t   *error)
{
   bson_uint32_t len;
   struct iovec *iov;
   ssize_t ret;
   int iovcnt;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(sd > -1, FALSE);

   switch (event->type) {
   case MONGOC_OPCODE_QUERY:
      SCATTER_QUERY(event, iov, iovcnt);
      SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      SCATTER_UPDATE(event, iov, iovcnt);
      SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      SCATTER_INSERT(event, iov, iovcnt);
      SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      SCATTER_GET_MORE(event, iov, iovcnt);
      SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      SCATTER_DELETE(event, iov, iovcnt);
      SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      SCATTER_KILL_CURSORS(event, iov, iovcnt);
      SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      SCATTER_MSG(event, iov, iovcnt);
      SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      SCATTER_REPLY(event, iov, iovcnt);
      SWAB_REPLY(event);
      break;
   default:
      return FALSE;
   }

   len = event->any.len;

   if (len > MONGOC_EVENT_MAX_LEN) {
      bson_set_error(error,
                     1, /* MONGOC_CLIENT_ERROR, */
                     1, /* MONGOC_CLIENT_ERROR_MSG_TOO_LARGE, */
                     "The event length is too large: %u",
                     len);
      return FALSE;
   }

   SWAB_HEADER(event);

   errno = 0;

   if (len == (ret = writev(sd, iov, iovcnt))) {
      return TRUE;
   }

   /*
    * TODO: Handle short write or write failure.
    */

   return FALSE;
}


bson_bool_t
mongoc_client_send (mongoc_client_t *client,
                    mongoc_event_t  *event,
                    bson_error_t    *error)
{
   bson_bool_t ret = FALSE;

   bson_return_val_if_fail(client, FALSE);
   bson_return_val_if_fail(event, FALSE);

   event->any.opcode = event->type;
   event->any.response_to = -1;
   event->any.request_id = ++client->request_id;

   ret = mongoc_event_write(event, client->outfd, error);

   return ret;
}


mongoc_client_t *
mongoc_client_new (const char *uri_string)
{
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   if (!(uri = mongoc_uri_new(uri_string))) {
      return NULL;
   }

   client = bson_malloc0(sizeof *client);
   client->uri = uri;
   client->outfd = 1;
   client->request_id = rand();

   return client;
}


mongoc_client_t *
mongoc_client_new_from_uri (const mongoc_uri_t *uri)
{
   return mongoc_client_new(mongoc_uri_get_string(uri));
}
