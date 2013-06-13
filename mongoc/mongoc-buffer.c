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
#include <stdarg.h>

#include "mongoc-error.h"
#include "mongoc-buffer-private.h"


#ifndef MONGOC_BUFFER_DEFAULT_SIZE
#define MONGOC_BUFFER_DEFAULT_SIZE 1024
#endif


/**
 * mongoc_buffer_init:
 * @buffer: A mongoc_buffer_t to initialize.
 * @buf: A data buffer to attach to @buffer.
 * @buflen: The size of @buflen.
 * @realloc_func: A function to resize @buf.
 *
 * Initializes @buffer for use. If additional space is needed by @buffer, then
 * @realloc_func will be called to resize @buf.
 *
 * @buffer takes ownership of @buf and will realloc it to zero bytes when
 * cleaning up the data structure.
 */
void
mongoc_buffer_init (mongoc_buffer_t   *buffer,
                    bson_uint8_t      *buf,
                    size_t             buflen,
                    bson_realloc_func  realloc_func)
{
   bson_return_if_fail(buffer);
   bson_return_if_fail(buf || !buflen);

   if (!realloc_func) {
      realloc_func = bson_realloc;
   }

   if (!buf || !buflen) {
      buf = realloc_func(NULL, MONGOC_BUFFER_DEFAULT_SIZE);
      buflen = MONGOC_BUFFER_DEFAULT_SIZE;
   }

   memset(buffer, 0, sizeof *buffer);

   buffer->data = buf;
   buffer->datalen = buflen;
   buffer->len = 0;
   buffer->off = 0;
   buffer->realloc_func = realloc_func;
}


/**
 * mongoc_buffer_destroy:
 * @buffer: A mongoc_buffer_t.
 *
 * Cleanup after @buffer and release any allocated resources.
 */
void
mongoc_buffer_destroy (mongoc_buffer_t *buffer)
{
   bson_return_if_fail(buffer);

   if (buffer->data) {
      buffer->realloc_func(buffer->data, 0);
      memset(buffer, 0, sizeof *buffer);
   }
}


bson_bool_t
mongoc_buffer_fill (mongoc_buffer_t *buffer,
                    mongoc_stream_t *stream,
                    size_t           minsize,
                    bson_error_t    *error)
{
   struct iovec iov;
   ssize_t toread = minsize;
   ssize_t ret;

   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(buffer->realloc_func, FALSE);
   bson_return_val_if_fail(stream, FALSE);
   bson_return_val_if_fail(minsize, FALSE);
   bson_return_val_if_fail(error, FALSE);

   /*
    * Fast path for cases where there is no work to do.
    */
   if (BSON_LIKELY(buffer->len >= minsize)) {
      return TRUE;
   }

   /*
    * If we do not have enough space to read the rest of the message at our
    * current position in the buffer, then move the buffer data to the
    * beginning of the buffer.
    */
   if (BSON_UNLIKELY((buffer->datalen - buffer->off) < minsize)) {
      memmove(buffer->data, buffer->data + buffer->off, buffer->len);
      buffer->off = 0;
   }

   /*
    * If the buffer is not big enough to hold the fill size, then resize the
    * buffer to contain it.
    */
   if (BSON_UNLIKELY(buffer->datalen < minsize)) {
      while (buffer->datalen < minsize) {
         buffer->datalen = MAX(32, buffer->datalen << 1);
      }
      buffer->data = buffer->realloc_func(buffer->data, buffer->datalen);
   }

   /*
    * Fill the buffer with bytes from the stream until we have an error or
    * minsize has been met.
    */
   while (toread > 0) {
      iov.iov_base = buffer->data + buffer->off + buffer->len;
      iov.iov_len = buffer->datalen - (buffer->off + buffer->len);
      errno = 0;
      ret = mongoc_stream_readv(stream, &iov, 1);
      if (ret <= 0) {
         bson_set_error(error,
                        MONGOC_ERROR_STREAM,
                        errno,
                        "Failed to read from stream: %s",
                        strerror(errno));
         return FALSE;
      }
      toread -= ret;
      buffer->len += ret;
   }

   return TRUE;
}


ssize_t
mongoc_buffer_readv (mongoc_buffer_t *buffer,
                     struct iovec    *iov,
                     size_t           iovcnt)
{
   bson_uint32_t i;
   ssize_t ret = 0;
   size_t len;

   bson_return_val_if_fail(buffer, -1);
   bson_return_val_if_fail(iov, -1);
   bson_return_val_if_fail(iovcnt, -1);

   for (i = 0; i < iovcnt; i++) {
      len = MIN(buffer->len, iov[i].iov_len);
      memcpy(iov[i].iov_base, &buffer->data[buffer->off], len);
      buffer->off += len;
      buffer->len -= len;
      ret += len;
   }

   return ret;
}


bson_bool_t
mongoc_buffer_read_typed (mongoc_buffer_t *buffer,
                          int              first_type,
                          void            *first_ptr,
                          ...)
{
   struct iovec iov;
   bson_int64_t v64;
   bson_int32_t v32;
   bson_bool_t found;
   va_list args;
   size_t end;
   void *ptr;
   int type;
   int i;

   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(first_type, FALSE);
   bson_return_val_if_fail(first_ptr, FALSE);

   ptr = first_ptr;
   type = first_type;

   va_start(args, first_ptr);
   do {
      switch (type) {
      case MONGOC_BUFFER_INT32:
         iov.iov_base = &v32;
         iov.iov_len = 4;
         if (!mongoc_buffer_readv(buffer, &iov, 1)) {
            return FALSE;
         }
         *(bson_int32_t *)ptr = BSON_UINT32_FROM_LE(v32);
         break;
      case MONGOC_BUFFER_INT64:
         iov.iov_base = &v64;
         iov.iov_len = 8;
         if (!mongoc_buffer_readv(buffer, &iov, 1)) {
            return FALSE;
         }
         *(bson_int64_t *)ptr = BSON_UINT64_FROM_LE(v64);
         break;
      case MONGOC_BUFFER_CSTRING:
         *(const char **)ptr = (const char *)&buffer->data[buffer->off];
         end = buffer->off + buffer->len;
         found = FALSE;
         for (i = buffer->off; i < end; i++) {
            if (!buffer->data[i]) {
               if (i + 1 < end) {
                  buffer->off = i + 1;
                  buffer->len = end - buffer->off;
               } else {
                  buffer->len = 0;
                  buffer->off = 0;
               }
               found = TRUE;
               break;
            }
         }
         if (!found) {
            return FALSE;
         }
         break;
      default:
         return FALSE;
      }
   } while ((type = va_arg(args, int)) && (ptr = va_arg(args, void*)));
   va_end(args);

   return TRUE;
}
