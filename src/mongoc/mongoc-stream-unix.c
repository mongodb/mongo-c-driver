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

#include "mongoc-counters-private.h"
#include "mongoc-compat.h"
#include "mongoc-log.h"
#include "mongoc-stream-unix.h"
#include "mongoc-stream-private.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-unix"


#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif


typedef struct
{
   mongoc_stream_t  stream;
   mongoc_fd_t        fd;
} mongoc_stream_unix_t;


static void
mongoc_stream_unix_destroy (mongoc_stream_t *stream)
{
   ENTRY;

   bson_return_if_fail(stream);

   if (mongoc_stream_close(stream) != 0) {
      /*
       * On Linux the close() system call can be pre-empted. However,
       * Linux says that you should not handle the failure since it can
       * cause a new file-descriptor being open()'d that raced with you
       * to be closed as well. Fun!
       */
   }

   bson_free(stream);

   mongoc_counter_streams_active_dec();
   mongoc_counter_streams_disposed_inc();

   EXIT;
}


static int
mongoc_stream_unix_close (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int ret = 0;

   ENTRY;

   bson_return_val_if_fail(stream, -1);

   if (mongoc_fd_is_valid(file->fd)) {
      if (!(ret = mongoc_close(file->fd))) {
         file->fd = MONGOC_FD_INVALID;
      }
   }

   RETURN(ret);
}

static int
mongoc_stream_unix_flush (mongoc_stream_t *stream)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int ret = 0;

   ENTRY;

   bson_return_val_if_fail(stream, -1);

   if (mongoc_fd_is_valid(file->fd)) {
      /*
       * TODO: You can't use fsync() with a socket (AFAIK). So we might
       *       need to select() for a writable condition or something to
       *       determine when data has been sent.
       */
#if 0
      ret = fsync(file->fd);
#endif
   }

   RETURN(ret);
}


static BSON_INLINE void
timeval_add_msec (struct timeval *tv,
                  uint32_t   msec)
{
   tv->tv_sec += msec / 1000U;
   tv->tv_usec += msec % 1000U;
   tv->tv_sec += tv->tv_usec / USEC_PER_SEC;
   tv->tv_usec %= USEC_PER_SEC;
}


static ssize_t
mongoc_stream_unix_readv (mongoc_stream_t *stream,
                          struct iovec    *iov,
                          size_t           iovcnt,
                          size_t           min_bytes,
                          int32_t     timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   mongoc_msghdr_t msg;
   mongoc_pollfd_t fds;
   int64_t now;
   int64_t expire;
   size_t cur = 0;
   ssize_t r;
   ssize_t ret = 0;
   int flags = 0;
   int timeout;
   int successful_read = 0;

   ENTRY;

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

   if (! mongoc_fd_is_valid(file->fd)) {
      errno = EBADF;
      RETURN(-1);
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
   fds.revents = 0;

   for (;;) {
      /*
       * Build our message for recvmsg() taking into account that we may have
       * already done a short-read and must increment the iovec.
       */
      memset(&msg, 0, sizeof msg);
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = iov + cur;
      msg.msg_iovlen = iovcnt - cur;

      BSON_ASSERT(msg.msg_iov->iov_len);
      BSON_ASSERT(cur < iovcnt);

      /*
       * Perform recvmsg() on socket to receive available data. If it turns
       * out this is not a socket, fall back to readv(). This should only
       * happen during unit tests.
       */
      errno = 0;
      r = mongoc_recvmsg(file->fd, &msg, flags);
      if (r < 0) {
         if (errno == ENOTSOCK) {
            r = mongoc_readv(file->fd, iov + cur, (int)(iovcnt - cur));
            if (!r) {
               RETURN(ret);
            }
         }
         if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            r = 0;
            GOTO(prepare_wait_poll);
         }
      }

      successful_read = 1;

      /*
       * If our recvmsg() failed, we can't do much now can we?
       */
      if (r < 0) {
         RETURN(r);
      } else if ((r == 0) && (fds.revents & POLLIN)) {
         /*
          * We expected input data and got zero. client closed?
          */
         RETURN(ret);
      } else {
         ret += r;
      }

      /*
       * Handle the case where we ran out of time and no data was read.
       */
prepare_wait_poll:
      now = bson_get_monotonic_time();
      if (((expire - now) < 0) && (r == 0)) {
         mongoc_counter_streams_timeout_inc();
         errno = ETIMEDOUT;
         RETURN(-1);
      }

      /*
       * Increment iovec's in the case we got a short read. Break out if
       * we have read all our expected data.
       */
      while ((cur < iovcnt) && (r >= (ssize_t)iov[cur].iov_len)) {
         BSON_ASSERT(iov[cur].iov_len);
         r -= iov[cur++].iov_len;
         BSON_ASSERT(cur <= iovcnt);
      }
      if (cur == iovcnt) {
         break;
      }
      BSON_ASSERT(cur < iovcnt);
      iov[cur].iov_base = ((uint8_t *)iov[cur].iov_base) + r;
      iov[cur].iov_len -= r;
      BSON_ASSERT(iovcnt - cur);
      BSON_ASSERT(iov[cur].iov_len);

      /*
       * If we got enough bytes to satisfy the minimum requirement, short
       * circuit so we don't potentially block on poll().
       */
      if (successful_read && ret >= (ssize_t)min_bytes) {
         break;
      }

      /*
       * Determine number of milliseconds until timeout expires.
       */
      timeout = (int)MAX(0, (expire - now) / 1000L);

      /*
       * Block on poll() until data is available or timeout. Upont timeout,
       * synthesize an errno of ETIMEDOUT.
       */
      errno = 0;
      fds.revents = 0;
      r = mongoc_poll(&fds, 1, timeout);
      if (r == -1) {
         RETURN(-1);
      } else if (r == 0) {
         errno = ETIMEDOUT;
         mongoc_counter_streams_timeout_inc();
         RETURN(-1);
      } else if ((fds.revents & POLLIN) != POLLIN) {
         RETURN(-1);
      }
   }

   mongoc_counter_streams_ingress_add(ret);

   RETURN(ret);
}


static ssize_t
mongoc_stream_unix_writev (mongoc_stream_t *stream,
                           struct iovec    *iov,
                           size_t           iovcnt,
                           int32_t     timeout_msec)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   mongoc_msghdr_t msg;
   mongoc_pollfd_t fds;
   int64_t now;
   int64_t expire;
   size_t cur = 0;
   ssize_t r;
   ssize_t ret = 0;
   int flags = 0;
   int timeout;

   ENTRY;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   /*
    * NOTE: See notes from mongoc_stream_unix_readv(), the apply here too.
    */

   if (! mongoc_fd_is_valid(file->fd)) {
      errno = EBADF;
      RETURN(-1);
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
   fds.revents = 0;

   for (;;) {
      /*
       * Build our message for recvmsg() taking into account that we may have
       * already done a short-read and must increment the iovec.
       */
      memset(&msg, 0, sizeof msg);
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      msg.msg_iov = iov + cur;
      msg.msg_iovlen = iovcnt - cur;

      BSON_ASSERT(msg.msg_iov->iov_len);
      BSON_ASSERT(cur < iovcnt);

      /*
       * Perform sendmsg() on socket to send next chunk of data. If it turns
       * out this is not a socket, fall back to writev(). This should only
       * happen during unit tests.
       */
   again:
      errno = 0;
      r = mongoc_sendmsg(file->fd, &msg, flags);
      if (r == -1) {
         if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            GOTO(again);
         } else if (errno == ENOTSOCK) {
            r = mongoc_writev(file->fd, iov + cur, (int)(iovcnt - cur));
            if (!r) {
               RETURN(ret);
            }
         }
      }

      /*
       * If our recvmsg() failed, we can't do much now can we.
       */
      if (r == -1) {
         RETURN(r);
      } else if ((r == 0) && (fds.revents & POLLOUT)) {
         /*
          * We expected output data and got zero. client closed?
          */
         RETURN(ret);
      } else {
         ret += r;
      }

      BSON_ASSERT(cur < iovcnt);

      /*
       * Increment iovec's in the case we got a short read. Break out if
       * we have read all our expected data.
       */
      while ((cur < iovcnt) && (r >= (ssize_t)iov[cur].iov_len)) {
         BSON_ASSERT(iov[cur].iov_len);
         r -= iov[cur++].iov_len;
         BSON_ASSERT(cur <= iovcnt);
      }
      if (cur == iovcnt) {
         break;
      }
      iov[cur].iov_base = ((uint8_t *)iov[cur].iov_base) + r;
      iov[cur].iov_len -= r;

      BSON_ASSERT(iovcnt - cur);
      BSON_ASSERT(iov[cur].iov_len);

      /*
       * Determine number of milliseconds until timeout expires.
       */
      now = bson_get_monotonic_time();
      timeout = (int)MAX(0, (expire - now) / 1000L);

      /*
       * Block on poll() until data is available or timeout. Upont timeout,
       * synthesize an errno of ETIMEDOUT.
       */
      errno = 0;
      fds.revents = 0;
      r = mongoc_poll(&fds, 1, timeout);
      if (r == -1) {
         RETURN(-1);
      } else if (r == 0) {
         errno = ETIMEDOUT;
         mongoc_counter_streams_timeout_inc();
         RETURN(-1);
      }
   }

   mongoc_counter_streams_egress_add(ret);

   RETURN(ret);
}


static int
mongoc_stream_unix_cork (mongoc_stream_t *stream)
{
#if defined(__linux__) || defined(TCP_NOPUSH)
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int state = 1;
   int ret = 0;

   ENTRY;

   bson_return_val_if_fail(stream, -1);

#if defined(__linux__)
   ret = mongoc_setsockopt(file->fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#elif defined(TCP_NOPUSH)
   ret = mongoc_setsockopt(file->fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#endif

   RETURN(ret);
#else
   RETURN(0);
#endif
}


static int
mongoc_stream_unix_uncork (mongoc_stream_t *stream)
{
#if defined(__linux__) || defined(TCP_NOPUSH)
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;
   int state = 0;
   int ret = 0;

   ENTRY;

   bson_return_val_if_fail(stream, -1);

#if defined(__linux__)
   ret = mongoc_setsockopt(file->fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#elif defined(TCP_NOPUSH)
   ret = mongoc_setsockopt(file->fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#endif

   RETURN(ret);
#else
   RETURN(0);
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
   int ret;

   ENTRY;

   bson_return_val_if_fail(file, -1);

   ret = mongoc_setsockopt(file->fd, level, optname, optval, optlen);

   RETURN(ret);
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
mongoc_stream_unix_new (mongoc_fd_t fd)
{
   mongoc_stream_unix_t *stream;

   ENTRY;

   /*
    * If we cannot put the file-descriptor in O_NONBLOCK mode, there isn't much
    * we can do. Just fail.
    */
   if (mongoc_fd_set_nonblock(fd) == -1) {
         MONGOC_WARNING("Failed to set O_NONBLOCK on file descriptor!");
         RETURN(NULL);
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

   RETURN((mongoc_stream_t *)stream);
}
