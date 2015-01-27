/*
 * Copyright 2013 MongoDB Inc.
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


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_file_page"

#include "mongoc-gridfs-file-page.h"
#include "mongoc-gridfs-file-page-private.h"

#include "mongoc-trace.h"


/** create a new page from a buffer
 *
 * The buffer should stick around for the life of the page
 */
mongoc_gridfs_file_page_t *
_mongoc_gridfs_file_page_new (const uint8_t *data,
                              uint32_t       len,
                              uint32_t       chunk_size)
{
   mongoc_gridfs_file_page_t *page;

   ENTRY;

   BSON_ASSERT (data);
   BSON_ASSERT (len <= chunk_size);

   page = bson_malloc0 (sizeof *page);

   page->chunk_size = chunk_size;
   page->read_buf = data;
   page->len = len;

   RETURN (page);
}


bool
_mongoc_gridfs_file_page_seek (mongoc_gridfs_file_page_t *page,
                               uint32_t              offset)
{
   ENTRY;

   BSON_ASSERT (page);

   BSON_ASSERT (offset <= page->len);

   page->offset = offset;

   RETURN (1);
}


int32_t
_mongoc_gridfs_file_page_read (mongoc_gridfs_file_page_t *page,
                               void                      *dst,
                               uint32_t              len)
{
   int bytes_read;
   const uint8_t *src;

   ENTRY;

   BSON_ASSERT (page);
   BSON_ASSERT (dst);

   bytes_read = BSON_MIN (len, page->len - page->offset);

   src = page->read_buf ? page->read_buf : page->buf;

   memcpy (dst, src + page->offset, bytes_read);

   page->offset += bytes_read;

   RETURN (bytes_read);
}


/**
 * _mongoc_gridfs_file_page_write:
 *
 * writes to a page
 *
 * writes are copy on write as regards the buf passed during construction.
 * I.e. the first write allocs a buf large enough for the chunk_size, which
 * because authoritative from then on out
 */
int32_t
_mongoc_gridfs_file_page_write (mongoc_gridfs_file_page_t *page,
                                const void                *src,
                                uint32_t              len)
{
   int bytes_written;

   ENTRY;

   BSON_ASSERT (page);
   BSON_ASSERT (src);

   bytes_written = BSON_MIN (len, page->chunk_size - page->offset);

   if (!page->buf) {
      page->buf = bson_malloc (page->chunk_size);
      memcpy (page->buf, page->read_buf, BSON_MIN (page->chunk_size, page->len));
   }

   memcpy (page->buf + page->offset, src, bytes_written);
   page->offset += bytes_written;

   page->len = BSON_MAX (page->offset, page->len);

   RETURN (bytes_written);
}


const uint8_t *
_mongoc_gridfs_file_page_get_data (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->buf ? page->buf : page->read_buf);
}


uint32_t
_mongoc_gridfs_file_page_get_len (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->len);
}


uint32_t
_mongoc_gridfs_file_page_tell (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->offset);
}


bool
_mongoc_gridfs_file_page_is_dirty (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   BSON_ASSERT (page);

   RETURN (page->buf ? 1 : 0);
}


void
_mongoc_gridfs_file_page_destroy (mongoc_gridfs_file_page_t *page)
{
   ENTRY;

   if (page->buf) { bson_free (page->buf); }

   bson_free (page);

   EXIT;
}
