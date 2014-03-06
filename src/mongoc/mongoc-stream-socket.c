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


typedef struct
{
   mongoc_stream_t  vtable;
   mongoc_socket_t *sock;
} mongoc_stream_socket_t;


static int
_mongoc_stream_socket_close (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   if (ss->sock) {
      return mongoc_socket_close (ss->sock);
   }

   return 0;
}


static void
_mongoc_stream_socket_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   if (ss->sock) {
      mongoc_socket_destroy (ss->sock);
      ss->sock = NULL;
   }

   bson_free (ss);
}


static int
_mongoc_stream_socket_setsockopt (mongoc_stream_t *stream,
                                  int              level,
                                  int              optname,
                                  void            *optval,
                                  socklen_t        optlen)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   if (ss->sock) {
      return mongoc_socket_setsockopt (ss->sock, level, optname,
                                       optval, optlen);
   }

   return 0;
}


static int
_mongoc_stream_socket_flush (mongoc_stream_t *stream)
{
   return 0;
}


static ssize_t
_mongoc_stream_socket_writev (mongoc_stream_t *stream,
                              struct iovec    *iov,
                              size_t           iovcnt,
                              int32_t          timeout_msec)
{
   mongoc_stream_socket_t *ss = (mongoc_stream_socket_t *)stream;

   if (ss->sock) {
      return mongoc_socket_sendv (ss->sock, iov, iovcnt, timeout_msec);
   }

   return -1;
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
   stream->vtable.writev = _mongoc_stream_socket_writev;
   stream->vtable.setsockopt = _mongoc_stream_socket_setsockopt;

   return (mongoc_stream_t *)stream;
}
