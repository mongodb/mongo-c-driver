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


#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "mongoc-array-private.h"
#include "mongoc-buffer-private.h"
#include "mongoc-error.h"
#include "mongoc-flags.h"
#include "mongoc-opcode.h"
#include "mongoc-rpc-private.h"
#include "mongoc-stream.h"


#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(_f) _f
#endif


#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif


typedef struct
{
   mongoc_stream_t  stream;
   int              fd;
} mongoc_stream_unix_t;


typedef struct
{
   mongoc_stream_t  stream;
   mongoc_stream_t *base_stream;
   mongoc_buffer_t  buffer;
} mongoc_stream_buffered_t;


static void
mongoc_stream_unix_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;

   bson_return_if_fail(stream);

   if (mongoc_stream_close(stream) != 0) {
      /*
       * TODO: Handle close failure.
       */
   }

   file->fd = -1;

   bson_free(stream);
}


static int
mongoc_stream_unix_close (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int ret = 0;

   bson_return_val_if_fail(stream, -1);

   if (file->fd != -1) {
      if (!(ret = close(file->fd))) {
         file->fd = -1;
      }
   }

   return ret;
}

static int
mongoc_stream_unix_flush (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int ret = 0;

   bson_return_val_if_fail(stream, -1);

   if (file->fd != -1) {
      /*
       * TODO: You can't use fsync() with a socket (AFAIK). So we might
       *       need to select() for a writable condition or something to
       *       determine when data has been sent.
       */
#if 0
      ret = fsync(file->fd);
#endif
   }

   return ret;
}


static BSON_INLINE void
timeval_add_msec (struct timeval *tv,
                  bson_uint32_t   msec)
{
   BSON_ASSERT(tv);

   tv->tv_sec += msec / 1000U;
   tv->tv_usec += msec % 1000U;
   tv->tv_sec += tv->tv_usec / USEC_PER_SEC;
   tv->tv_usec %= USEC_PER_SEC;
}


static bson_uint32_t
msec_until (const struct timeval *tv)
{
   struct timeval now;
   bson_uint64_t tv_msec;
   bson_uint64_t now_msec;

   gettimeofday(&now, NULL);
   tv_msec = (tv->tv_sec * 1000UL) + (tv->tv_usec / 1000UL);
   now_msec = (now.tv_sec * 1000UL) + (now.tv_usec / 1000UL);

   return (now_msec >= tv_msec) ? 0 : (tv_msec - now_msec);
}


static ssize_t
mongoc_stream_unix_readv (mongoc_stream_t *stream,
                          struct iovec    *iov,
                          size_t           iovcnt,
                          bson_uint32_t    timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   struct timeval expire = { 0 };
   struct pollfd fds;
   int timeout;
   int count;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   if (file->fd == -1) {
      errno = EBADF;
      return -1;
   }

   fds.fd = file->fd;
   fds.events = (POLLIN | POLLERR);
#ifdef POLLRDHUP
   fds.events |= POLLRDHUP;
#endif
   fds.revents = 0;

   gettimeofday(&expire, NULL);
   timeval_add_msec(&expire, timeout_msec);

   timeout = msec_until(&expire);
   fds.revents = 0;

   count = poll(&fds, 1, timeout);
   if (!count) {
      /*
       * TODO: Handle read timeout.
       */
   }

#ifdef TEMP_FAILURE_RETRY
   return TEMP_FAILURE_RETRY(readv(file->fd, iov, iovcnt));
#else
   return readv(file->fd, iov, iovcnt);
#endif
}


static ssize_t
mongoc_stream_unix_writev (mongoc_stream_t *stream,
                           struct iovec    *iov,
                           size_t           iovcnt,
                           bson_uint32_t    timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   struct msghdr msg;
   size_t cur = 0;
   ssize_t written;
   ssize_t ret = 0;
   int flags = 0;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   /*
    * TODO: Handle timeout_msec.
    */

   for (;;) {
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = iov + cur;
      msg.msg_iovlen = iovcnt - cur;
      msg.msg_control = NULL;
      msg.msg_controllen = 0;
      msg.msg_flags = 0;

      BSON_ASSERT(msg.msg_iov->iov_len);
      BSON_ASSERT(cur < iovcnt);

      errno = 0;
      written = TEMP_FAILURE_RETRY(sendmsg(file->fd, &msg, flags));
      if (written < 0) {
         return -1;
      }

      ret += written;

      BSON_ASSERT(cur < iovcnt);

      while ((cur < iovcnt) && (written >= iov[cur].iov_len)) {
         BSON_ASSERT(iov[cur].iov_len);
         written -= iov[cur++].iov_len;
         BSON_ASSERT(cur <= iovcnt);
      }

      if (cur == iovcnt) {
         break;
      }

      iov[cur].iov_base = ((bson_uint8_t *)iov[cur].iov_base) + written;
      iov[cur].iov_len -= written;

      BSON_ASSERT(iovcnt - cur);
      BSON_ASSERT(iov[cur].iov_len);
   }

   return ret;
}


static int
mongoc_stream_unix_cork (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int state = 1;
   bson_return_val_if_fail(stream, -1);
#ifdef __linux__
   return setsockopt(file->fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#else
   return setsockopt(file->fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#endif
}


static int
mongoc_stream_unix_uncork (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int state = 0;
   bson_return_val_if_fail(stream, -1);
#ifdef __linux__
   return setsockopt(file->fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#else
   return setsockopt(file->fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#endif
}


/**
 * mongoc_stream_close:
 * @stream: A mongoc_stream_t.
 *
 * Closes the underlying file-descriptor used by @stream.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_close (mongoc_stream_t *stream)
{
   bson_return_val_if_fail(stream, -1);
   return stream->close(stream);
}


/**
 * mongoc_stream_destroy:
 * @stream: A mongoc_stream_t.
 *
 * Frees any resources referenced by @stream, including the memory allocation
 * for @stream.
 */
void
mongoc_stream_destroy (mongoc_stream_t *stream)
{
   bson_return_if_fail(stream);
   stream->destroy(stream);
}


/**
 * mongoc_stream_flush:
 * @stream: A mongoc_stream_t.
 *
 * Flushes the data in the underlying stream to the transport.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_flush (mongoc_stream_t *stream)
{
   bson_return_val_if_fail(stream, -1);
   return stream->flush(stream);
}


/**
 * mongoc_stream_writev:
 * @stream: A mongoc_stream_t.
 * @iov: An array of iovec to write to the stream.
 * @iovcnt: The number of elements in @iov.
 *
 * Writes an array of iovec buffers to the underlying stream.
 *
 * Returns: the number of bytes written, or -1 upon failure.
 */
ssize_t
mongoc_stream_writev (mongoc_stream_t *stream,
                      struct iovec    *iov,
                      size_t           iovcnt,
                      bson_uint32_t    timeout_msec)
{
   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   BSON_ASSERT(stream->writev);

   return stream->writev(stream, iov, iovcnt, timeout_msec);
}


/**
 * mongoc_stream_readv:
 * @stream: A mongoc_stream_t.
 * @iov: An array of iovec containing the location and sizes to read.
 * @iovcnt: the number of elements in @iov.
 *
 * Reads into the various buffers pointed to by @iov and associated
 * buffer lengths.
 *
 * Returns: the number of bytes read or -1 on failure.
 */
ssize_t
mongoc_stream_readv (mongoc_stream_t *stream,
                     struct iovec    *iov,
                     size_t           iovcnt,
                     bson_uint32_t    timeout_msec)
{
   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   BSON_ASSERT(stream->readv);

   return stream->readv(stream, iov, iovcnt, timeout_msec);
}


/**
 * mongoc_stream_read:
 * @stream: A mongoc_stream_t.
 * @buf: A buffer to write into.
 * @count: The number of bytes to write into @buf.
 *
 * Simplified access to mongoc_stream_readv(). Creates a single iovec
 * with the buffer provided.
 *
 * Returns: -1 on failure, otherwise the number of bytes read.
 */
ssize_t
mongoc_stream_read (mongoc_stream_t *stream,
                    void            *buf,
                    size_t           count,
                    bson_uint32_t    timeout_msec)
{
   struct iovec iov;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(buf, -1);

   if (!count) {
      return 0;
   }

   iov.iov_base = buf;
   iov.iov_len = count;

   BSON_ASSERT(stream->readv);

   return stream->readv(stream, &iov, 1, timeout_msec);
}


/**
 * mongoc_stream_cork:
 * @stream: (in): A mongoc_stream_t.
 *
 * Corks a stream, preventing packets from being sent immediately. This is
 * useful if you need to send multiple messages together as a single packet.
 *
 * Call mongoc_stream_uncork() after writing your data.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_cork (mongoc_stream_t *stream)
{
   bson_return_val_if_fail(stream, -1);
   return stream->cork ? stream->cork(stream) : 0;
}


/**
 * mongoc_stream_uncork:
 * @stream: (in): A mongoc_stream_t.
 *
 * Uncorks a stream, previously corked with mongoc_stream_cork().
 *
 * Returns: 0 on success, -1 on failure.
 */
int
mongoc_stream_uncork (mongoc_stream_t *stream)
{
   bson_return_val_if_fail(stream, -1);
   return stream->uncork ? stream->uncork(stream) : 0;
}


/**
 * mongoc_stream_new_from_unix:
 * @fd: A unix style file-descriptor.
 *
 * Create a new mongoc_stream_t for a UNIX file descriptor. This is
 * expected to be a socket of some sort (such as a UNIX or TCP socket).
 *
 * This may be useful after having connected to a peer to provide a
 * higher level API for reading and writing. It also allows for
 * interoperability with external stream abstractions in higher level
 * languages.
 *
 * Returns: A newly allocated mongoc_stream_t that should be freed with
 *   mongoc_stream_destroy().
 */
mongoc_stream_t *
mongoc_stream_new_from_unix (int fd)
{
   mongoc_stream_unix_t *stream;
   int flags;

   bson_return_val_if_fail(fd != -1, NULL);

   /*
    * If we cannot put the file-descriptor in O_NONBLOCK mode, there isn't much
    * we can do. Just fail.
    */
   flags = fcntl(fd, F_GETFD);
   fcntl(fd, F_SETFD, flags | O_NONBLOCK);

   stream = bson_malloc0(sizeof *stream);
   stream->fd = fd;
   stream->stream.destroy = mongoc_stream_unix_destroy;
   stream->stream.close = mongoc_stream_unix_close;
   stream->stream.flush = mongoc_stream_unix_flush;
   stream->stream.writev = mongoc_stream_unix_writev;
   stream->stream.readv = mongoc_stream_unix_readv;
   stream->stream.cork = mongoc_stream_unix_cork;
   stream->stream.uncork = mongoc_stream_unix_uncork;

   return (mongoc_stream_t *)stream;
}


static void
mongoc_stream_buffered_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;

   bson_return_if_fail(stream);

   mongoc_stream_destroy(buffered->base_stream);
   mongoc_buffer_destroy(&buffered->buffer);
   bson_free(stream);
}


static int
mongoc_stream_buffered_close (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(stream, -1);
   return mongoc_stream_close(buffered->base_stream);
}


static int
mongoc_stream_buffered_flush (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(buffered, -1);
   return mongoc_stream_flush(buffered->base_stream);
}


static ssize_t
mongoc_stream_buffered_writev (mongoc_stream_t *stream,
                               struct iovec    *iov,
                               size_t           iovcnt,
                               bson_uint32_t    timeout_msec)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;

   bson_return_val_if_fail(buffered, -1);

   /*
    * TODO: Implement buffering.
    */

   return mongoc_stream_writev(buffered->base_stream, iov, iovcnt, timeout_msec);
}


static ssize_t
mongoc_stream_buffered_readv (mongoc_stream_t *stream,
                              struct iovec    *iov,
                              size_t           iovcnt,
                              bson_uint32_t    timeout_msec)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;

   bson_return_val_if_fail(buffered, -1);

   /*
    * TODO: Implement buffering.
    */

   return mongoc_stream_readv(buffered->base_stream, iov, iovcnt, timeout_msec);
}


mongoc_stream_t *
mongoc_stream_buffered_new (mongoc_stream_t *base_stream)
{
   mongoc_stream_buffered_t *stream;

   bson_return_val_if_fail(base_stream, NULL);

   stream = bson_malloc0(sizeof *stream);
   stream->stream.destroy = mongoc_stream_buffered_destroy;
   stream->stream.close = mongoc_stream_buffered_close;
   stream->stream.flush = mongoc_stream_buffered_flush;
   stream->stream.writev = mongoc_stream_buffered_writev;
   stream->stream.readv = mongoc_stream_buffered_readv;
   stream->base_stream = base_stream;
   mongoc_buffer_init(&stream->buffer, NULL, 0, NULL);

   return (mongoc_stream_t *)stream;
}
