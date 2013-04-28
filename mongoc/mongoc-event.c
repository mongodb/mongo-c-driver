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


#include <errno.h>

#include "mongoc-event-private.h"


bson_bool_t
mongoc_event_encode (mongoc_event_t     *event,
                     bson_uint8_t      **buf,
                     size_t             *buflen,
                     bson_realloc_func   realloc_func,
                     bson_error_t       *error)
{
   bson_uint32_t len;
   bson_uint32_t i;
   bson_uint32_t iovcnt;
   bson_uint8_t *dst;
   struct iovec *iov;

   bson_return_val_if_fail(event, FALSE);
   bson_return_val_if_fail(buf, FALSE);
   bson_return_val_if_fail(buflen, FALSE);

   if (!realloc_func) {
      realloc_func = bson_realloc;
   }

   switch (event->type) {
   case MONGOC_OPCODE_QUERY:
      MONGOC_EVENT_SCATTER_QUERY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      MONGOC_EVENT_SCATTER_UPDATE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      MONGOC_EVENT_SCATTER_INSERT(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      MONGOC_EVENT_SCATTER_GET_MORE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      MONGOC_EVENT_SCATTER_DELETE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      MONGOC_EVENT_SCATTER_KILL_CURSORS(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      MONGOC_EVENT_SCATTER_MSG(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      MONGOC_EVENT_SCATTER_REPLY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_REPLY(event);
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

   MONGOC_EVENT_SWAB_HEADER(event);

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
      MONGOC_EVENT_SCATTER_QUERY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_QUERY(event);
      break;
   case MONGOC_OPCODE_UPDATE:
      MONGOC_EVENT_SCATTER_UPDATE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_UPDATE(event);
      break;
   case MONGOC_OPCODE_INSERT:
      MONGOC_EVENT_SCATTER_INSERT(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_INSERT(event);
      break;
   case MONGOC_OPCODE_GET_MORE:
      MONGOC_EVENT_SCATTER_GET_MORE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_GET_MORE(event);
      break;
   case MONGOC_OPCODE_DELETE:
      MONGOC_EVENT_SCATTER_DELETE(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_DELETE(event);
      break;
   case MONGOC_OPCODE_KILL_CURSORS:
      MONGOC_EVENT_SCATTER_KILL_CURSORS(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_KILL_CURSORS(event);
      break;
   case MONGOC_OPCODE_MSG:
      MONGOC_EVENT_SCATTER_MSG(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_MSG(event);
      break;
   case MONGOC_OPCODE_REPLY:
      MONGOC_EVENT_SCATTER_REPLY(event, iov, iovcnt);
      MONGOC_EVENT_SWAB_REPLY(event);
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

   MONGOC_EVENT_SWAB_HEADER(event);

   errno = 0;

   if (len == (ret = writev(sd, iov, iovcnt))) {
      return TRUE;
   }

   /*
    * TODO: Handle short write or write failure.
    */

   return FALSE;
}
