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

#include "mongoc-stream.h"


typedef struct
{
   mongoc_stream_t stream;
   int             fd;
} mongoc_stream_unix_t;


static void
mongoc_stream_unix_destroy (mongoc_stream_t *stream)
{
   bson_return_if_fail(stream);

   if (!!mongoc_stream_close(stream)) {
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
      ret = fsync(file->fd);
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
 * mongoc_stream_new_unix:
 * @path: The path to a UNIX domain socket.
 *
 * Create a new mongoc_stream_t for a UNIX domain socket found on the
 * local file-system. If the socket does not exist, NULL is returned.
 *
 * Returns: A newly allocated mongoc_stream_t that should be freed with
 *   mongoc_stream_destroy().
 */
mongoc_stream_t *
mongoc_stream_new_unix (const char *path)
{
   mongoc_stream_unix_t *stream;
   int fd = -1;

   bson_return_val_if_fail(path, NULL);

   /*
    * TODO: open unix domain socket.
    */

   if (fd == -1) {
      return NULL;
   }

   stream = bson_malloc0(sizeof *stream);
   stream->fd = fd;
   stream->stream.destroy = mongoc_stream_unix_destroy;
   stream->stream.close = mongoc_stream_unix_close;
   stream->stream.flush = mongoc_stream_unix_flush;
   stream->stream.writev = mongoc_stream_unix_writev;
   stream->stream.readv = mongoc_stream_unix_readv;

   return (mongoc_stream_t *)stream;
}
