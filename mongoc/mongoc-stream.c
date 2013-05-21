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
#include <unistd.h>

#include "mongoc-buffer-private.h"
#include "mongoc-error.h"
#include "mongoc-event-private.h"
#include "mongoc-stream.h"


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
   bson_bool_t      closed;
} mongoc_stream_buffered_t;


static void
mongoc_stream_unix_destroy (mongoc_stream_t *stream)
{
   bson_return_if_fail(stream);

   if (mongoc_stream_close(stream) != 0) {
      /*
       * TODO: Handle close failure.
       */
   }

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


static ssize_t
mongoc_stream_unix_readv (mongoc_stream_t *stream,
                          struct iovec    *iov,
                          size_t           iovcnt)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   if (file->fd == -1) {
      errno = EBADF;
      return -1;
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
                           size_t           iovcnt)
{
   mongoc_stream_unix_t *file = (mongoc_stream_unix_t *)stream;

   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

#ifdef TEMP_FAILURE_RETRY
   return TEMP_FAILURE_RETRY(writev(file->fd, iov, iovcnt));
#else
   return writev(file->fd, iov, iovcnt);
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
                      size_t           iovcnt)
{
   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   return stream->writev(stream, iov, iovcnt);
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
                     size_t           iovcnt)
{
   bson_return_val_if_fail(stream, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   return stream->readv(stream, iov, iovcnt);
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

   bson_return_val_if_fail(fd != -1, NULL);

   stream = bson_malloc0(sizeof *stream);
   stream->fd = fd;
   stream->stream.destroy = mongoc_stream_unix_destroy;
   stream->stream.close = mongoc_stream_unix_close;
   stream->stream.flush = mongoc_stream_unix_flush;
   stream->stream.writev = mongoc_stream_unix_writev;
   stream->stream.readv = mongoc_stream_unix_readv;

   return (mongoc_stream_t *)stream;
}


static void
mongoc_stream_buffered_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;

   bson_return_if_fail(stream);

   if (!buffered->closed) {
      mongoc_stream_close(stream);
   }

   mongoc_buffer_destroy(&buffered->buffer);
}


static int
mongoc_stream_buffered_close (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   int ret;

   bson_return_val_if_fail(stream, -1);

   if (-1 != (ret = mongoc_stream_close(buffered->base_stream))) {
      buffered->closed = TRUE;
   }

   return ret;
}


static int
mongoc_stream_buffered_flush (mongoc_stream_t *stream)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(stream, -1);
   return mongoc_stream_flush(buffered->base_stream);
}


static ssize_t
mongoc_stream_buffered_writev (mongoc_stream_t *stream,
                               struct iovec    *iov,
                               size_t           iovcnt)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_return_val_if_fail(stream, -1);
   return mongoc_stream_writev(buffered->base_stream, iov, iovcnt);
}


static ssize_t
mongoc_stream_buffered_readv (mongoc_stream_t *stream,
                              struct iovec    *iov,
                              size_t           iovcnt)
{
   mongoc_stream_buffered_t *buffered = (mongoc_stream_buffered_t *)stream;
   bson_uint32_t i;
   size_t count = 0;

   bson_return_val_if_fail(stream, -1);

   for (i = 0; i < iovcnt; i++) {
      count += iov[i].iov_len;
   }

   mongoc_buffer_fill(&buffered->buffer, stream, count, NULL);
   return mongoc_buffer_readv(&buffered->buffer, iov, iovcnt);
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


/**
 * mongoc_stream_ismaster:
 * @stream: A mongoc_stream_t.
 * @error: A location for an error.
 *
 * This is a convenience function to run the "ismaster" command on the given
 * stream. Upon failure, NULL is returned and @error is set.
 *
 * Returns: A bson_t that should be freed with bson_destroy().
 */
bson_t *
mongoc_stream_ismaster (mongoc_stream_t *stream,
                        bson_error_t    *error)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_QUERY);
   bson_t q;

   bson_return_val_if_fail(stream, FALSE);
   bson_return_val_if_fail(error, FALSE);

   bson_init(&q);
   bson_append_int32(&q, "ismaster", 8, 1);

   ev.query.flags = MONGOC_QUERY_SLAVE_OK;
   ev.query.ns = "admin.$cmd";
   ev.query.nslen = 10;
   ev.query.skip = 0;
   ev.query.n_return = 1;
   ev.query.query = &q;
   ev.query.fields = NULL;

   if (!mongoc_event_write(&ev, stream, error)) {
      return FALSE;
   }

   bson_destroy(&q);

   if (!mongoc_event_read(&ev, stream, error)) {
      return FALSE;
   }

   /*
    * TODO: Check request_id/response_to?
    */

   if (ev.type != MONGOC_OPCODE_REPLY || ev.reply.docslen != 1) {
      /*
       * TODO: Set error.
       */
      return FALSE;
   }

   /*
    * TODO: Determine how we want to release incoming events.
    */

   return ev.reply.docs[0];
}
