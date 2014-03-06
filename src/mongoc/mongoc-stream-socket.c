/*
 * Copyright 2014 MongoDB, Inc.
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


#include "mongoc-stream-private.h"
#include "mongoc-stream-socket.h"
#include "mongoc-trace.h"


typedef struct
{
   mongoc_stream_t  vtable;
   mongoc_socket_t *sock;
} mongoc_stream_socket_t;


static int
_mongoc_stream_socket_close (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (ss, -1);

   if (ss->sock) {
      ret = mongoc_socket_close (ss->sock);
      RETURN (ret);
   }

   RETURN (0);
}


static void
_mongoc_stream_socket_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   ENTRY;

   bson_return_if_fail (ss);

   if (ss->sock) {
      mongoc_socket_destroy (ss->sock);
      ss->sock = NULL;
   }

   bson_free (ss);

   EXIT;
}


static int
_mongoc_stream_socket_setsockopt (mongoc_stream_t *stream,
                                  int              level,
                                  int              optname,
                                  void            *optval,
                                  socklen_t        optlen)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (ss, -1);
   bson_return_val_if_fail (ss->sock, -1);

   ret = mongoc_socket_setsockopt (ss->sock, level, optname, optval, optlen);

   RETURN (ret);
}


static int
_mongoc_stream_socket_flush (mongoc_stream_t *stream)
{
   ENTRY;
   RETURN (0);
}


static ssize_t
_mongoc_stream_socket_readv (mongoc_stream_t *stream,
                             mongoc_iovec_t  *iov,
                             size_t           iovcnt,
                             size_t           min_bytes,
                             int32_t          timeout_msec)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   ssize_t ret = 0;
   ssize_t nread;
   size_t cur = 0;

   ENTRY;

   bson_return_val_if_fail (ss, -1);
   bson_return_val_if_fail (ss->sock, -1);

   /*
    * This isn't ideal, we should plumb through to recvmsg(), but we
    * don't actually use this in any way but to a single buffer
    * currently anyway, so should be just fine.
    */

   for (;;) {
      nread = mongoc_socket_recv (ss->sock,
                                  iov [cur].iov_base,
                                  iov [cur].iov_len,
                                  0,
                                  timeout_msec);

      if (nread == -1) {
         if (ret >= min_bytes) {
            RETURN (ret);
         }
         RETURN (-1);
      }

      ret += nread;

      while ((cur < iovcnt) && (nread >= (ssize_t)iov [cur].iov_len)) {
         nread -= iov [cur++].iov_len;
      }

      if (cur == iovcnt) {
         break;
      }

      if (ret >= min_bytes) {
         RETURN (ret);
      }

      iov [cur].iov_base = ((uint8_t *)iov [cur].iov_base) + nread;
      iov [cur].iov_len -= nread;

      BSON_ASSERT (iovcnt - cur);
      BSON_ASSERT (iov [cur].iov_len);
   }

   RETURN (ret);
}


static ssize_t
_mongoc_stream_socket_writev (mongoc_stream_t *stream,
                              mongoc_iovec_t  *iov,
                              size_t           iovcnt,
                              int32_t          timeout_msec)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;
   ssize_t ret;

   ENTRY;

   if (ss->sock) {
      ret = mongoc_socket_sendv (ss->sock, iov, iovcnt, timeout_msec);
      RETURN (ret);
   }

   RETURN (-1);
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_stream_socket_new --
 *
 *       Create a new mongoc_stream_t using the mongoc_socket_t for
 *       read and write underneath.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

mongoc_stream_t *
mongoc_stream_socket_new (mongoc_socket_t *sock) /* IN */
{
   mongoc_stream_socket_t *stream;

   bson_return_val_if_fail (sock, NULL);

   stream = bson_malloc0 (sizeof *stream);
   stream->vtable.close = _mongoc_stream_socket_close;
   stream->vtable.destroy = _mongoc_stream_socket_destroy;
   stream->vtable.flush = _mongoc_stream_socket_flush;
   stream->vtable.readv = _mongoc_stream_socket_readv;
   stream->vtable.writev = _mongoc_stream_socket_writev;
   stream->vtable.setsockopt = _mongoc_stream_socket_setsockopt;
   stream->sock = sock;

   return (mongoc_stream_t *)stream;
}
