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


#define SPACE_FOR(_b, _sz) (((_b)->datalen - (_b)->off - (_b)->len) >= _sz)


static BSON_INLINE bson_uint32_t
npow2 (bson_uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;

   return v;
}


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


/**
 * mongoc_buffer_clear:
 * @buffer: A mongoc_buffer_t.
 * @zero: If the memory should be zeroed.
 *
 * Clears a buffers contents and resets it to initial state. You can request
 * that the memory is zeroed, which might be useful if you know the contents
 * contain security related information.
 */
void
mongoc_buffer_clear (mongoc_buffer_t *buffer,
                     bson_bool_t      zero)
{
   bson_return_if_fail(buffer);

   if (zero) {
      memset(buffer->data, 0, buffer->datalen);
   }

   buffer->off = 0;
   buffer->len = 0;
}


/**
 * mongoc_buffer_append_from_stream:
 * @buffer; A mongoc_buffer_t.
 * @stream: The stream to read from.
 * @size: The number of bytes to read.
 * @error: A location for a bson_error_t, or NULL.
 *
 * Reads from stream @size bytes and stores them in @buffer. This can be used
 * in conjunction with reading RPCs from a stream. You read from the stream
 * into this buffer and then scatter the buffer into the RPC.
 *
 * Returns: TRUE if successful; otherwise FALSE and @error is set.
 */
bson_bool_t
mongoc_buffer_append_from_stream (mongoc_buffer_t *buffer,
                                  mongoc_stream_t *stream,
                                  size_t           size,
                                  bson_error_t    *error)
{
   ssize_t ret;

   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(stream, FALSE);
   bson_return_val_if_fail(size, FALSE);

   BSON_ASSERT(buffer->datalen);

   if (!SPACE_FOR(buffer, size)) {
      memmove(&buffer->data[0], &buffer->data[buffer->off], buffer->len);
      buffer->off = 0;
      if (!SPACE_FOR(buffer, size)) {
         buffer->datalen = npow2(buffer->datalen);
         buffer->data = bson_realloc(buffer->data, buffer->datalen);
      }
   }

   ret = mongoc_stream_read(stream, &buffer->data[buffer->off + buffer->len], size);
   if (ret != size) {
      bson_set_error(error,
                     MONGOC_ERROR_STREAM,
                     MONGOC_ERROR_STREAM_SOCKET,
                     "Failed to read %llu bytes from socket.",
                     (unsigned long long)size);
      return FALSE;
   }

   buffer->len += ret;

   return TRUE;
}


/**
 * mongoc_buffer_fill:
 * @buffer: A mongoc_buffer_t.
 * @stream: A stream to read from.
 * @error: A location for a bson_error_t or NULL.
 *
 * Attempts to fill the entire buffer.
 *
 * Returns: The number of buffered bytes, or -1 on failure.
 */
ssize_t
mongoc_buffer_fill (mongoc_buffer_t *buffer,
                    mongoc_stream_t *stream,
                    bson_error_t    *error)
{
   ssize_t ret;
   size_t size;

   bson_return_val_if_fail(buffer, FALSE);
   bson_return_val_if_fail(stream, FALSE);

   memmove(buffer->data, &buffer->data[buffer->off], buffer->len);
   buffer->off = 0;
   size = buffer->datalen - buffer->len;
   ret = mongoc_stream_read(stream, &buffer[buffer->off + buffer->len], size);
   if (ret >= 0) {
      buffer->len += ret;
      return buffer->len;
   }
   return ret;
}
