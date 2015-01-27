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
#define MONGOC_LOG_DOMAIN "gridfs_file"

#include <limits.h>
#include <time.h>
#include <errno.h>

#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-collection.h"
#include "mongoc-gridfs.h"
#include "mongoc-gridfs-private.h"
#include "mongoc-gridfs-file.h"
#include "mongoc-gridfs-file-private.h"
#include "mongoc-gridfs-file-page.h"
#include "mongoc-gridfs-file-page-private.h"
#include "mongoc-iovec.h"
#include "mongoc-trace.h"

static bool
_mongoc_gridfs_file_refresh_page (mongoc_gridfs_file_t *file);

static bool
_mongoc_gridfs_file_flush_page (mongoc_gridfs_file_t *file);


/*****************************************************************
* Magic accessor generation
*
* We need some accessors to get and set properties on files, to handle memory
* ownership and to determine dirtiness.  These macros produce the getters and
* setters we need
*****************************************************************/

#define MONGOC_GRIDFS_FILE_STR_ACCESSOR(name) \
   const char * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
   { \
      return file->name ? file->name : file->bson_##name; \
   } \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const char           *str)  \
   { \
      if (file->name) { \
         bson_free (file->name); \
      } \
      file->name = bson_strdup (str); \
      file->is_dirty = 1; \
   }

#define MONGOC_GRIDFS_FILE_BSON_ACCESSOR(name) \
   const bson_t * \
   mongoc_gridfs_file_get_##name (mongoc_gridfs_file_t * file) \
   { \
      if (file->name.len) { \
         return &file->name; \
      } else if (file->bson_##name.len) { \
         return &file->bson_##name; \
      } else { \
         return NULL; \
      } \
   } \
   void \
      mongoc_gridfs_file_set_##name (mongoc_gridfs_file_t * file, \
                                     const bson_t * bson) \
   { \
      if (file->name.len) { \
         bson_destroy (&file->name); \
      } \
      bson_copy_to (bson, &(file->name)); \
      file->is_dirty = 1; \
   }

MONGOC_GRIDFS_FILE_STR_ACCESSOR (md5)
MONGOC_GRIDFS_FILE_STR_ACCESSOR (filename)
MONGOC_GRIDFS_FILE_STR_ACCESSOR (content_type)
MONGOC_GRIDFS_FILE_BSON_ACCESSOR (aliases)
MONGOC_GRIDFS_FILE_BSON_ACCESSOR (metadata)


/** save a gridfs file */
bool
mongoc_gridfs_file_save (mongoc_gridfs_file_t *file)
{
   bson_t *selector, *update, child;
   const char *md5;
   const char *filename;
   const char *content_type;
   const bson_t *aliases;
   const bson_t *metadata;
   bool r;

   ENTRY;

   if (!file->is_dirty) {
      return 1;
   }

   if (file->page && _mongoc_gridfs_file_page_is_dirty (file->page)) {
      _mongoc_gridfs_file_flush_page (file);
   }

   md5 = mongoc_gridfs_file_get_md5 (file);
   filename = mongoc_gridfs_file_get_filename (file);
   content_type = mongoc_gridfs_file_get_content_type (file);
   aliases = mongoc_gridfs_file_get_aliases (file);
   metadata = mongoc_gridfs_file_get_metadata (file);

   selector = bson_new ();
   bson_append_value (selector, "_id", -1, &file->files_id);

   update = bson_new ();
   bson_append_document_begin (update, "$set", -1, &child);
   bson_append_int64 (&child, "length", -1, file->length);
   bson_append_int32 (&child, "chunkSize", -1, file->chunk_size);
   bson_append_date_time (&child, "uploadDate", -1, file->upload_date);

   if (md5) {
      bson_append_utf8 (&child, "md5", -1, md5, -1);
   }

   if (filename) {
      bson_append_utf8 (&child, "filename", -1, filename, -1);
   }

   if (content_type) {
      bson_append_utf8 (&child, "contentType", -1, content_type, -1);
   }

   if (aliases) {
      bson_append_array (&child, "aliases", -1, aliases);
   }

   if (metadata) {
      bson_append_document (&child, "metadata", -1, metadata);
   }

   bson_append_document_end (update, &child);

   r = mongoc_collection_update (file->gridfs->files, MONGOC_UPDATE_UPSERT,
                                 selector, update, NULL, &file->error);

   file->failed = !r;


   bson_destroy (selector);
   bson_destroy (update);

   file->is_dirty = 0;

   RETURN (r);
}


/**
 * _mongoc_gridfs_file_new_from_bson:
 *
 * creates a gridfs file from a bson object
 *
 * This is only really useful for instantiating a gridfs file from a server
 * side object
 */
mongoc_gridfs_file_t *
_mongoc_gridfs_file_new_from_bson (mongoc_gridfs_t *gridfs,
                                   const bson_t    *data)
{
   mongoc_gridfs_file_t *file;
   const bson_value_t *value;
   const char *key;
   bson_iter_t iter;
   const uint8_t *buf;
   uint32_t buf_len;

   ENTRY;

   BSON_ASSERT (gridfs);
   BSON_ASSERT (data);

   file = bson_malloc0 (sizeof *file);

   file->gridfs = gridfs;
   bson_copy_to (data, &file->bson);

   bson_iter_init (&iter, &file->bson);

   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);

      if (0 == strcmp (key, "_id")) {
         value = bson_iter_value (&iter);
         bson_value_copy (value, &file->files_id);
      } else if (0 == strcmp (key, "length")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter) &&
             !BSON_ITER_HOLDS_DOUBLE (&iter)) {
            GOTO (failure);
         }
         file->length = bson_iter_as_int64 (&iter);
      } else if (0 == strcmp (key, "chunkSize")) {
         if (!BSON_ITER_HOLDS_INT32 (&iter) &&
             !BSON_ITER_HOLDS_INT64 (&iter) &&
             !BSON_ITER_HOLDS_DOUBLE (&iter)) {
            GOTO (failure);
         }
         if (bson_iter_as_int64 (&iter) > INT32_MAX) {
            GOTO (failure);
         }
         file->chunk_size = (int32_t)bson_iter_as_int64 (&iter);
      } else if (0 == strcmp (key, "uploadDate")) {
         if (!BSON_ITER_HOLDS_DATE_TIME (&iter)){
            GOTO (failure);
         }
         file->upload_date = bson_iter_date_time (&iter);
      } else if (0 == strcmp (key, "md5")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_md5 = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "filename")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_filename = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "contentType")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            GOTO (failure);
         }
         file->bson_content_type = bson_iter_utf8 (&iter, NULL);
      } else if (0 == strcmp (key, "aliases")) {
         if (!BSON_ITER_HOLDS_ARRAY (&iter)) {
            GOTO  (failure);
         }
         bson_iter_array (&iter, &buf_len, &buf);
         bson_init_static (&file->bson_aliases, buf, buf_len);
      } else if (0 == strcmp (key, "metadata")) {
         if (!BSON_ITER_HOLDS_DOCUMENT (&iter)) {
            GOTO (failure);
         }
         bson_iter_document (&iter, &buf_len, &buf);
         bson_init_static (&file->bson_metadata, buf, buf_len);
      }
   }

   /* TODO: is there are a minimal object we should be verifying that we
    * actually have here? */

   RETURN (file);

failure:
   bson_destroy (&file->bson);

   RETURN (NULL);
}


/**
 * _mongoc_gridfs_file_new:
 *
 * Create a new empty gridfs file
 */
mongoc_gridfs_file_t *
_mongoc_gridfs_file_new (mongoc_gridfs_t          *gridfs,
                         mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;
   mongoc_gridfs_file_opt_t default_opt = { 0 };

   ENTRY;

   BSON_ASSERT (gridfs);

   if (!opt) {
      opt = &default_opt;
   }

   file = bson_malloc0 (sizeof *file);

   file->gridfs = gridfs;
   file->is_dirty = 1;

   if (opt->chunk_size) {
      file->chunk_size = opt->chunk_size;
   } else {
      /*
       * The default chunk size is now 255kb. This used to be 256k but has been
       * reduced to allow for them to fit within power of two sizes in mongod.
       *
       * See CDRIVER-322.
       */
      file->chunk_size = (1 << 18) - 1024;
   }

   file->files_id.value_type = BSON_TYPE_OID;
   bson_oid_init (&file->files_id.value.v_oid, NULL);

   file->upload_date = time (NULL) * 1000;

   if (opt->md5) {
      file->md5 = bson_strdup (opt->md5);
   }

   if (opt->filename) {
      file->filename = bson_strdup (opt->filename);
   }

   if (opt->content_type) {
      file->content_type = bson_strdup (opt->content_type);
   }

   if (opt->aliases) {
      bson_copy_to (opt->aliases, &(file->aliases));
   }

   if (opt->metadata) {
      bson_copy_to (opt->metadata, &(file->metadata));
   }

   RETURN (file);
}

void
mongoc_gridfs_file_destroy (mongoc_gridfs_file_t *file)
{
   ENTRY;

   BSON_ASSERT (file);

   if (file->page) {
      _mongoc_gridfs_file_page_destroy (file->page);
   }

   if (file->bson.len) {
      bson_destroy (&file->bson);
   }

   if (file->cursor) {
      mongoc_cursor_destroy (file->cursor);
   }

   if (file->files_id.value_type) {
      bson_value_destroy (&file->files_id);
   }

   if (file->md5) {
      bson_free (file->md5);
   }

   if (file->filename) {
      bson_free (file->filename);
   }

   if (file->content_type) {
      bson_free (file->content_type);
   }

   if (file->aliases.len) {
      bson_destroy (&file->aliases);
   }

   if (file->bson_aliases.len) {
      bson_destroy (&file->bson_aliases);
   }

   if (file->metadata.len) {
      bson_destroy (&file->metadata);
   }

   if (file->bson_metadata.len) {
      bson_destroy (&file->bson_metadata);
   }

   bson_free (file);

   EXIT;
}


/** readv against a gridfs file */
ssize_t
mongoc_gridfs_file_readv (mongoc_gridfs_file_t *file,
                          mongoc_iovec_t       *iov,
                          size_t                iovcnt,
                          size_t                min_bytes,
                          uint32_t              timeout_msec)
{
   uint32_t bytes_read = 0;
   int32_t r;
   size_t i;
   uint32_t iov_pos;

   ENTRY;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (timeout_msec <= INT_MAX);

   /* TODO: we should probably do something about timeout_msec here */

   if (!file->page) {
      _mongoc_gridfs_file_refresh_page (file);
   }

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      for (;; ) {
         r = _mongoc_gridfs_file_page_read (file->page,
                                           (uint8_t *)iov[i].iov_base + iov_pos,
                                           (uint32_t)(iov[i].iov_len - iov_pos));
         BSON_ASSERT (r >= 0);

         iov_pos += r;
         file->pos += r;
         bytes_read += r;

         if (iov_pos == iov[i].iov_len) {
            /* filled a bucket, keep going */
            break;
         } else if (file->length == file->pos) {
            /* we're at the end of the file.  So we're done */
            RETURN (bytes_read);
         } else if (bytes_read >= min_bytes) {
            /* we need a new page, but we've read enough bytes to stop */
            RETURN (bytes_read);
         } else {
            /* more to read, just on a new page */
            _mongoc_gridfs_file_refresh_page (file);
         }
      }
   }

   RETURN (bytes_read);
}


/** writev against a gridfs file */
ssize_t
mongoc_gridfs_file_writev (mongoc_gridfs_file_t *file,
                           mongoc_iovec_t       *iov,
                           size_t                iovcnt,
                           uint32_t              timeout_msec)
{
   uint32_t bytes_written = 0;
   int32_t r;
   size_t i;
   uint32_t iov_pos;

   ENTRY;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);
   BSON_ASSERT (timeout_msec <= INT_MAX);

   /* TODO: we should probably do something about timeout_msec here */

   for (i = 0; i < iovcnt; i++) {
      iov_pos = 0;

      for (;; ) {
         if (!file->page) {
            _mongoc_gridfs_file_refresh_page (file);
         }

         r = _mongoc_gridfs_file_page_write (file->page,
                                            (uint8_t *)iov[i].iov_base + iov_pos,
                                            (uint32_t)(iov[i].iov_len - iov_pos));
         BSON_ASSERT (r >= 0);

         iov_pos += r;
         file->pos += r;
         bytes_written += r;

         file->length = BSON_MAX (file->length, (int64_t)file->pos);

         if (iov_pos == iov[i].iov_len) {
            /** filled a bucket, keep going */
            break;
         } else {
            /** flush the buffer, the next pass through will bring in a new page
             *
             * Our file pointer is now on the new page, so push it back one so
             * that flush knows to flush the old page rather than a new one.
             * This is a little hacky
             */
            file->pos--;
            _mongoc_gridfs_file_flush_page (file);
            file->pos++;
         }
      }
   }

   file->is_dirty = 1;

   RETURN (bytes_written);
}


/** flush a gridfs file's current page to the db */
static bool
_mongoc_gridfs_file_flush_page (mongoc_gridfs_file_t *file)
{
   bson_t *selector, *update;
   bool r;
   const uint8_t *buf;
   uint32_t len;

   ENTRY;
   BSON_ASSERT (file);
   BSON_ASSERT (file->page);

   buf = _mongoc_gridfs_file_page_get_data (file->page);
   len = _mongoc_gridfs_file_page_get_len (file->page);

   selector = bson_new ();

   bson_append_value (selector, "files_id", -1, &file->files_id);
   bson_append_int32 (selector, "n", -1, (int32_t)(file->pos / file->chunk_size));

   update = bson_sized_new (file->chunk_size + 100);

   bson_append_value (update, "files_id", -1, &file->files_id);
   bson_append_int32 (update, "n", -1, (int32_t)(file->pos / file->chunk_size));
   bson_append_binary (update, "data", -1, BSON_SUBTYPE_BINARY, buf, len);

   r = mongoc_collection_update (file->gridfs->chunks, MONGOC_UPDATE_UPSERT,
                                 selector, update, NULL, &file->error);

   file->failed = !r;

   bson_destroy (selector);
   bson_destroy (update);

   if (r) {
      _mongoc_gridfs_file_page_destroy (file->page);
      file->page = NULL;
      r = mongoc_gridfs_file_save (file);
   }

   RETURN (r);
}


/** referesh a gridfs file's underlying page
 *
 * This unconditionally fetches the current page, even if the current page
 * covers the same theoretical chunk.
 */
static bool
_mongoc_gridfs_file_refresh_page (mongoc_gridfs_file_t *file)
{
   bson_t *query, *fields, child, child2;
   const bson_t *chunk;
   const char *key;
   bson_iter_t iter;

   uint32_t n;
   const uint8_t *data;
   uint32_t len;

   ENTRY;

   BSON_ASSERT (file);

   n = (uint32_t)(file->pos / file->chunk_size);

   if (file->page) {
      _mongoc_gridfs_file_page_destroy (file->page);
      file->page = NULL;
   }

   /* if the file pointer is past the end of the current file (I.e. pointing to
    * a new chunk) and we're on a chunk boundary, we'll pass the page
    * constructor a new empty page */
   if ((int64_t)file->pos >= file->length && !(file->pos % file->chunk_size)) {
      data = (uint8_t *)"";
      len = 0;
   } else {
      /* if we have a cursor, but the cursor doesn't have the chunk we're going
       * to need, destroy it (we'll grab a new one immediately there after) */
      if (file->cursor &&
          !(file->cursor_range[0] >= n && file->cursor_range[1] <= n)) {
         mongoc_cursor_destroy (file->cursor);
         file->cursor = NULL;
      }

      if (!file->cursor) {
         query = bson_new ();

         bson_append_document_begin(query, "$query", -1, &child);
            bson_append_value (&child, "files_id", -1, &file->files_id);

            bson_append_document_begin (&child, "n", -1, &child2);
               bson_append_int32 (&child2, "$gte", -1, (int32_t)(file->pos / file->chunk_size));
            bson_append_document_end (&child, &child2);
         bson_append_document_end(query, &child);

         bson_append_document_begin(query, "$orderby", -1, &child);
            bson_append_int32 (&child, "n", -1, 1);
         bson_append_document_end(query, &child);

         fields = bson_new ();
         bson_append_int32 (fields, "n", -1, 1);
         bson_append_int32 (fields, "data", -1, 1);
         bson_append_int32 (fields, "_id", -1, 0);

         /* find all chunks greater than or equal to our current file pos */
         file->cursor = mongoc_collection_find (file->gridfs->chunks,
                                                MONGOC_QUERY_NONE, 0, 0, 0, query,
                                                fields, NULL);

         file->cursor_range[0] = n;
         file->cursor_range[1] = (uint32_t)(file->length / file->chunk_size);

         bson_destroy (query);
         bson_destroy (fields);

         BSON_ASSERT (file->cursor);
      }

      /* we might have had a cursor before, then seeked ahead past a chunk.
       * iterate until we're on the right chunk */
      while (file->cursor_range[0] <= n) {
         if (!mongoc_cursor_next (file->cursor, &chunk)) {
            if (file->cursor->failed) {
               memcpy (&(file->error), &(file->cursor->error),
                       sizeof (bson_error_t));
               file->failed = true;
            }

            RETURN (0);
         }

         file->cursor_range[0]++;
      }

      bson_iter_init (&iter, chunk);

      /* grab out what we need from the chunk */
      while (bson_iter_next (&iter)) {
         key = bson_iter_key (&iter);

         if (strcmp (key, "n") == 0) {
            n = bson_iter_int32 (&iter);
         } else if (strcmp (key, "data") == 0) {
            bson_iter_binary (&iter, NULL, &len, &data);
         } else {
            RETURN (0);
         }
      }

      /* we're on the wrong chunk somehow... probably because our gridfs is
       * missing chunks.
       *
       * TODO: maybe we should make more noise here?
       */

      if (!(n == file->pos / file->chunk_size)) {
         return 0;
      }
   }

   file->page = _mongoc_gridfs_file_page_new (data, len, file->chunk_size);

   /* seek in the page towards wherever we're supposed to be */
   RETURN (_mongoc_gridfs_file_page_seek (file->page, file->pos %
                                         file->chunk_size));
}


/** Seek in a gridfs file to a given location
 *
 * @param whence is regular fseek whence.  I.e. SEEK_SET, SEEK_CUR or SEEK_END
 *
 */
int
mongoc_gridfs_file_seek (mongoc_gridfs_file_t *file,
                         int64_t               delta,
                         int                   whence)
{
   uint64_t offset;

   BSON_ASSERT(file);

   switch (whence) {
   case SEEK_SET:
      offset = delta;
      break;
   case SEEK_CUR:
      offset = file->pos + delta;
      break;
   case SEEK_END:
      offset = (file->length - 1) + delta;
      break;
   default:
      errno = EINVAL;
      return -1;

      break;
   }

   BSON_ASSERT (file->length > (int64_t)offset);

   if (offset / file->chunk_size != file->pos / file->chunk_size) {
      /** no longer on the same page */

      if (file->page) {
         if (_mongoc_gridfs_file_page_is_dirty (file->page)) {
            _mongoc_gridfs_file_flush_page (file);
         } else {
            _mongoc_gridfs_file_page_destroy (file->page);
            file->page = NULL;
         }
      }

      /** we'll pick up the seek when we fetch a page on the next action.  We lazily load */
   } else {
      _mongoc_gridfs_file_page_seek (file->page, offset % file->chunk_size);
   }

   file->pos = offset;

   return 0;
}

uint64_t
mongoc_gridfs_file_tell (mongoc_gridfs_file_t *file)
{
   BSON_ASSERT(file);

   return file->pos;
}

bool
mongoc_gridfs_file_error (mongoc_gridfs_file_t *file,
                          bson_error_t         *error)
{
   BSON_ASSERT(file);
   BSON_ASSERT(error);

   if (BSON_UNLIKELY(file->failed)) {
      bson_set_error(error,
                     file->error.domain,
                     file->error.code,
                     "%s",
                     file->error.message);
      RETURN(true);
   }

   RETURN(false);
}

int64_t
mongoc_gridfs_file_get_length (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->length;
}

int32_t
mongoc_gridfs_file_get_chunk_size (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->chunk_size;
}

int64_t
mongoc_gridfs_file_get_upload_date (mongoc_gridfs_file_t *file)
{
   bson_return_val_if_fail (file, -1);

   return file->upload_date;
}

bool
mongoc_gridfs_file_remove (mongoc_gridfs_file_t *file,
                           bson_error_t         *error)
{
   bson_t sel = BSON_INITIALIZER;
   bool ret = false;

   bson_return_val_if_fail (file, false);

   BSON_APPEND_VALUE (&sel, "_id", &file->files_id);

   if (!mongoc_collection_remove (file->gridfs->files,
                                  MONGOC_REMOVE_SINGLE_REMOVE,
                                  &sel,
                                  NULL,
                                  error)) {
      goto cleanup;
   }

   bson_reinit (&sel);
   BSON_APPEND_VALUE (&sel, "files_id", &file->files_id);

   if (!mongoc_collection_remove (file->gridfs->chunks,
                                  MONGOC_REMOVE_NONE,
                                  &sel,
                                  NULL,
                                  error)) {
      goto cleanup;
   }

   ret = true;

cleanup:
   bson_destroy (&sel);

   return ret;
}
