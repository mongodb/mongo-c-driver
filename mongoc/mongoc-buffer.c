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
   bson_return_if_fail(buf || realloc_func);

   if (!realloc_func) {
      realloc_func = bson_realloc;
   }

   memset(buffer, 0, sizeof *buffer);

   if (!buf) {
      buf = realloc_func(NULL, MONGOC_BUFFER_DEFAULT_SIZE);
      buflen = MONGOC_BUFFER_DEFAULT_SIZE;
   }

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

   buffer->realloc_func(buffer->data, 0);
   memset(buffer, 0, sizeof *buffer);
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
         buffer->datalen <<= 1;
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
      ret = mongoc_stream_readv(stream, &iov, 1);
      if (ret <= 0) {
         return FALSE;
      }
      toread -= ret;
      buffer->len += ret;
   }

   return TRUE;
}
