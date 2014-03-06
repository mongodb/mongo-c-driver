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
#include "mongoc-stream-file.h"
#include "mongoc-trace.h"


typedef struct
{
   mongoc_stream_t vtable;
   int             fd;
} mongoc_stream_file_t;


static int
_mongoc_stream_file_close (mongoc_stream_t *stream)
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;
   int ret;

   ENTRY;

   bson_return_val_if_fail (file, -1);

   if (file->fd != -1) {
      ret = close (file->fd);
      file->fd = -1;
      RETURN (ret);
   }

   RETURN (0);
}


static void
_mongoc_stream_file_destroy (mongoc_stream_t *stream)
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;

   ENTRY;

   bson_return_if_fail (file);

   if (file->fd) {
      _mongoc_stream_file_close (stream);
   }

   bson_free (file);

   EXIT;
}


static int
_mongoc_stream_file_flush (mongoc_stream_t *stream) /* IN */
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;

   BSON_ASSERT (file);

   if (file->fd != -1) {
#ifdef _WIN32
      return _commit (file->fd);
#else
      return fsync (file->fd);
#endif
   }

   return 0;
}


static ssize_t
_mongoc_stream_file_readv (mongoc_stream_t *stream,       /* IN */
                           mongoc_iovec_t  *iov,          /* IN */
                           size_t           iovcnt,       /* IN */
                           size_t           min_bytes,    /* IN */
                           int32_t          timeout_msec) /* IN */
{
   return -1;
}


static ssize_t
_mongoc_stream_file_writev (mongoc_stream_t *stream,       /* IN */
                            mongoc_iovec_t  *iov,          /* IN */
                            size_t           iovcnt,       /* IN */
                            int32_t          timeout_msec) /* IN */
{
   return -1;
}


mongoc_stream_t *
mongoc_stream_file_new (int fd) /* IN */
{
   mongoc_stream_file_t *stream;

   bson_return_val_if_fail (fd != -1, NULL);

   stream = bson_malloc0 (sizeof *stream);
   stream->vtable.close = _mongoc_stream_file_close;
   stream->vtable.destroy = _mongoc_stream_file_destroy;
   stream->vtable.flush = _mongoc_stream_file_flush;
   stream->vtable.readv = _mongoc_stream_file_readv;
   stream->vtable.writev = _mongoc_stream_file_writev;
   stream->fd = fd;

   return (mongoc_stream_t *)stream;
}


mongoc_stream_t *
mongoc_stream_file_new_for_path (const char *path,  /* IN */
                                 int         flags, /* IN */
                                 int         mode)  /* IN */
{
   int fd;

   bson_return_val_if_fail (path, NULL);

#ifdef _WIN32
   fd = _open (path, (flags | _O_BINARY), mode);
#else
   fd = open (path, flags, mode);
#endif

   if (fd == -1) {
      return NULL;
   }

   return mongoc_stream_file_new (fd);
}


int
mongoc_stream_file_get_fd (mongoc_stream_t *stream)
{
   mongoc_stream_file_t *file = (mongoc_stream_file_t *)stream;
   bson_return_val_if_fail (file, -1);
   return file->fd;
}
