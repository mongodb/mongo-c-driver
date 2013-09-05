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
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "mongoc-array-private.h"
#include "mongoc-buffer-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-flags.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-rpc-private.h"
#include "mongoc-stream.h"


#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif


#ifndef MONGOC_DEFAULT_TIMEOUT_MSEC
#define MONGOC_DEFAULT_TIMEOUT_MSEC (60L * 60L * 1000L)
#endif


typedef struct
{
   mongoc_stream_t  stream;
   int              fd;
} mongoc_stream_unix_t;


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

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();
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


static ssize_t
mongoc_stream_unix_readv (mongoc_stream_t *stream,
                          struct iovec    *iov,
                          size_t           iovcnt,
                          ssize_t          min_bytes,
                          bson_uint32_t    timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   struct msghdr msg;
   struct pollfd fds;
   bson_int64_t now;
   bson_int64_t expire;
   size_t cur = 0;
   ssize_t r;
   ssize_t ret = 0;
   int flags = 0;
   int timeout;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);
   bson_return_val_if_fail(timeout_msec <= INT_MAX, -1);

   /*
    * NOTE: Thar' be dragons.
    *
    * This function is complex. It combines vectored read from a socket or
    * file descriptor with a timeout as needed by the wtimeout requirements of
    * the MongoDB driver.
    *
    * poll() is used for it's portability in waiting for available I/O on a
    * non-blocking socket or file descriptor.
    *
    * To allow for vectored I/O operations on the descriptor, we use recvmsg()
    * instead of recv(). However, recvmsg() does not support regular
    * file-descriptors and therefore we must fallback to readv() if we detect
    * such a case. This is fine since we don't actually use regular
    * file-descriptors (just socket descriptors) during production use. Files
    * are only used in test cases.
    *
    * We apply a default timeout if one has not been provided. The default
    * is one hour. If this is not sufficient, try TCP over bongo drums.
    */

   if (file->fd == -1) {
      errno = EBADF;
      return -1;
   }

   if (!timeout_msec) {
      timeout_msec = MONGOC_DEFAULT_TIMEOUT_MSEC;
   }

   /*
    * We require a monotonic clock for determining out timeout interval. This
    * is so that we are resilient to changes in the underlying wall clock,
    * such as during timezone changes. The monotonic clock is in microseconds
    * since an unknown epoch (but often system startup).
    */
   expire = bson_get_monotonic_time() + (timeout_msec * 1000UL);

   /*
    * Prepare our pollfd. If POLLRDHUP is supported, we can get notified of
    * our peer hang-up.
    */
   fds.fd = file->fd;
   fds.events = (POLLIN | POLLERR | POLLHUP | POLLNVAL);
#ifdef POLLRDHUP
   fds.events |= POLLRDHUP;
#endif

   for (;;) {
      /*
       * Build our message for recvmsg() taking into account that we may have
       * already done a short-read and must increment the iovec.
       */
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = iov + cur;
      msg.msg_iovlen = iovcnt - cur;
      msg.msg_control = NULL;
      msg.msg_controllen = 0;
      msg.msg_flags = 0;

      BSON_ASSERT(msg.msg_iov->iov_len);
      BSON_ASSERT(cur < iovcnt);

      /*
       * Perform recvmsg() on socket to receive available data. If it turns
       * out this is not a socket, fall back to readv(). This should only
       * happen during unit tests.
       */
   again:
      errno = 0;
      r = recvmsg(file->fd, &msg, flags);
      if (r == -1) {
         if (errno == EAGAIN) {
            goto again;
         } else if (errno == ENOTSOCK) {
            r = readv(file->fd, iov + cur, iovcnt - cur);
            if (!r) {
               return ret;
            }
         }
      }

      /*
       * If our recvmsg() failed, we can't do much now can we?
       */
      if (r == -1) {
         return r;
      } else {
         ret += r;
      }

      /*
       * Handle the case where we ran out of time and no data was read.
       */
      now = bson_get_monotonic_time();
      if (((expire - now) < 0) && (r == 0)) {
         mongoc_counter_streams_timeout_inc();
         return -1;
      }

      /*
       * Increment iovec's in the case we got a short read. Break out if
       * we have read all our expected data.
       */
      while ((cur < iovcnt) && (r >= iov[cur].iov_len)) {
         BSON_ASSERT(iov[cur].iov_len);
         r -= iov[cur++].iov_len;
         BSON_ASSERT(cur <= iovcnt);
      }
      if (cur == iovcnt) {
         break;
      }
      BSON_ASSERT(cur < iovcnt);
      iov[cur].iov_base = ((bson_uint8_t *)iov[cur].iov_base) + r;
      iov[cur].iov_len -= r;
      BSON_ASSERT(iovcnt - cur);
      BSON_ASSERT(iov[cur].iov_len);

      /*
       * If we got enough bytes to satisfy the minimum requirement, short
       * circuit so we don't potentially block on poll().
       */
      if (ret >= min_bytes) {
         break;
      }

      /*
       * Determine number of milliseconds until timeout expires.
       */
      timeout = MAX(0, (expire - now) / 1000L);

      /*
       * Block on poll() until data is available or timeout. Upont timeout,
       * synthesize an errno of ETIMEDOUT.
       */
      errno = 0;
      fds.revents = 0;
      r = poll(&fds, 1, timeout);
      if (r == -1) {
         return -1;
      } else if (r == 0) {
         errno = ETIMEDOUT;
         mongoc_counter_streams_timeout_inc();
         return -1;
      }
   }

   mongoc_counter_streams_ingress_add(ret);

   return ret;
}


static ssize_t
mongoc_stream_unix_writev (mongoc_stream_t *stream,
                           struct iovec    *iov,
                           size_t           iovcnt,
                           bson_uint32_t    timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   struct msghdr msg;
   struct pollfd fds;
   bson_int64_t now;
   bson_int64_t expire;
   size_t cur = 0;
   ssize_t r;
   ssize_t ret = 0;
   int flags = 0;
   int timeout;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   /*
    * NOTE: See notes from mongoc_stream_unix_readv(), the apply here too.
    */

   if (file->fd == -1) {
      errno = EBADF;
      return -1;
   }

   if (!timeout_msec) {
      timeout_msec = MONGOC_DEFAULT_TIMEOUT_MSEC;
   }

   /*
    * We require a monotonic clock for determining out timeout interval. This
    * is so that we are resilient to changes in the underlying wall clock,
    * such as during timezone changes. The monotonic clock is in microseconds
    * since an unknown epoch (but often system startup).
    */
   expire = bson_get_monotonic_time() + (timeout_msec * 1000L);

   /*
    * Prepare our pollfd. If POLLRDHUP is supported, we can get notified of
    * our peer hang-up.
    */
   fds.fd = file->fd;
   fds.events = (POLLOUT | POLLERR | POLLHUP | POLLNVAL);

   for (;;) {
      /*
       * Build our message for recvmsg() taking into account that we may have
       * already done a short-read and must increment the iovec.
       */
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = iov + cur;
      msg.msg_iovlen = iovcnt - cur;
      msg.msg_control = NULL;
      msg.msg_controllen = 0;
      msg.msg_flags = 0;

      BSON_ASSERT(msg.msg_iov->iov_len);
      BSON_ASSERT(cur < iovcnt);

      /*
       * Perform sendmsg() on socket to send next chunk of data. If it turns
       * out this is not a socket, fall back to writev(). This should only
       * happen during unit tests.
       */
   again:
      errno = 0;
      r = sendmsg(file->fd, &msg, flags);
      if (r == -1) {
         if (errno == EAGAIN) {
            goto again;
         } else if (errno == ENOTSOCK) {
            r = writev(file->fd, iov + cur, iovcnt - cur);
            if (!r) {
               return ret;
            }
         }
      }

      /*
       * If our recvmsg() failed, we can't do much now can we.
       */
      if (r == -1) {
         return r;
      } else {
         ret += r;
      }

      BSON_ASSERT(cur < iovcnt);

      /*
       * Increment iovec's in the case we got a short read. Break out if
       * we have read all our expected data.
       */
      while ((cur < iovcnt) && (r >= iov[cur].iov_len)) {
         BSON_ASSERT(iov[cur].iov_len);
         r -= iov[cur++].iov_len;
         BSON_ASSERT(cur <= iovcnt);
      }
      if (cur == iovcnt) {
         break;
      }
      iov[cur].iov_base = ((bson_uint8_t *)iov[cur].iov_base) + r;
      iov[cur].iov_len -= r;

      BSON_ASSERT(iovcnt - cur);
      BSON_ASSERT(iov[cur].iov_len);

      /*
       * Determine number of milliseconds until timeout expires.
       */
      now = bson_get_monotonic_time();
      timeout = MAX(0, (expire - now) / 1000L);

      /*
       * Block on poll() until data is available or timeout. Upont timeout,
       * synthesize an errno of ETIMEDOUT.
       */
      errno = 0;
      fds.revents = 0;
      r = poll(&fds, 1, timeout);
      if (r == -1) {
         return -1;
      } else if (r == 0) {
         errno = ETIMEDOUT;
         mongoc_counter_streams_timeout_inc();
         return -1;
      }
   }

   mongoc_counter_streams_egress_add(ret);

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


static int
mongoc_stream_unix_setsockopt (mongoc_stream_t *stream,
                               int              level,
                               int              optname,
                               void            *optval,
                               socklen_t        optlen)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   bson_return_val_if_fail(file, -1);
   return setsockopt(file->fd, level, optname, optval, optlen);
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
 * @min_bytes: the minumum number of bytes to return, or -1.
 *
 * Reads into the various buffers pointed to by @iov and associated
 * buffer lengths.
 *
 * If @min_bytes is specified, then at least that number of bytes will
 * be returned or -1 for failure. This is useful for opportunistic
 * buffering.
 *
 * Returns: the number of bytes read or -1 on failure.
 */
ssize_t
mongoc_stream_readv (mongoc_stream_t *stream,
                     struct iovec    *iov,
                     size_t           iovcnt,
                     ssize_t          min_bytes,
                     bson_uint32_t    timeout_msec)
{
   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   BSON_ASSERT(stream->readv);

   return stream->readv(stream, iov, iovcnt, min_bytes, timeout_msec);
}


/**
 * mongoc_stream_read:
 * @stream: A mongoc_stream_t.
 * @buf: A buffer to write into.
 * @count: The number of bytes to write into @buf.
 * @min_bytes: The minimum number of bytes to receive, or -1.
 *
 * Simplified access to mongoc_stream_readv(). Creates a single iovec
 * with the buffer provided.
 *
 * If @min_bytes is not -1, the minimum number of bytes to receive
 * or an error is returned. This is useful for opportunistic buffering.
 *
 * Returns: -1 on failure, otherwise the number of bytes read.
 */
ssize_t
mongoc_stream_read (mongoc_stream_t *stream,
                    void            *buf,
                    size_t           count,
                    ssize_t          min_bytes,
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

   return stream->readv(stream, &iov, 1, min_bytes, timeout_msec);
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


int
mongoc_stream_setsockopt (mongoc_stream_t *stream,
                          int              level,
                          int              optname,
                          void            *optval,
                          socklen_t        optlen)
{
   bson_return_val_if_fail(stream, -1);

   if (stream->setsockopt) {
      return stream->setsockopt(stream, level, optname, optval, optlen);
   }

   return 0;
}


/**
 * mongoc_stream_unix_new:
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
mongoc_stream_unix_new (int fd)
{
   mongoc_stream_unix_t *stream;
   int flags;

   bson_return_val_if_fail(fd != -1, NULL);

   /*
    * If we cannot put the file-descriptor in O_NONBLOCK mode, there isn't much
    * we can do. Just fail.
    */
   flags = fcntl(fd, F_GETFL);
   if ((flags & O_NONBLOCK) != O_NONBLOCK) {
      if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
         MONGOC_WARNING("Failed to set O_NONBLOCK on file descriptor!");
         return NULL;
      }
   }

   stream = bson_malloc0(sizeof *stream);
   stream->fd = fd;
   stream->stream.destroy = mongoc_stream_unix_destroy;
   stream->stream.close = mongoc_stream_unix_close;
   stream->stream.flush = mongoc_stream_unix_flush;
   stream->stream.writev = mongoc_stream_unix_writev;
   stream->stream.readv = mongoc_stream_unix_readv;
   stream->stream.cork = mongoc_stream_unix_cork;
   stream->stream.uncork = mongoc_stream_unix_uncork;
   stream->stream.setsockopt = mongoc_stream_unix_setsockopt;

   mongoc_counter_streams_active_inc();

   return (mongoc_stream_t *)stream;
}
