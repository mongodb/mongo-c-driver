/*
 * Copyright 2018-present MongoDB, Inc.
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
#include <bson-types.h>
#include "mongoc.h"
#include "mongoc-gridfs-bucket.h"
#include "mongoc-gridfs-bucket-private.h"
#include "mongoc-gridfs-bucket-file-private.h"
#include "mongoc-stream-gridfs-upload-private.h"
#include "mongoc-write-concern-private.h"
#include "mongoc-stream-gridfs-download-private.h"
#include "mongoc-stream-private.h"
#include "mongoc-read-concern-private.h"

/*
 * Attempts to find the file corresponding to the given file_id in GridFS
 *
 * Returns NULL and sets the bucket error if the file doesn't exist.
 */
static bool
_mongoc_gridfs_find_file_with_id (mongoc_gridfs_bucket_t *bucket,
                                  const bson_value_t *file_id,
                                  bson_t *file,
                                  bson_error_t *error)
{
   mongoc_cursor_t *cursor;
   bson_t filter;
   const bson_t *doc;
   bool r;

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);

   bson_init (&filter);

   BSON_APPEND_VALUE (&filter, "_id", file_id);

   cursor =
      mongoc_collection_find_with_opts (bucket->files, &filter, NULL, NULL);
   bson_destroy (&filter);

   if (mongoc_cursor_error (cursor, error)) {
      mongoc_cursor_destroy (cursor);
      return false;
   }

   r = mongoc_cursor_next (cursor, &doc);
   if (!r) {
      mongoc_cursor_destroy (cursor);
      bson_set_error (error,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_INVALID_FILENAME,
                      "No file with given id exists");
   } else {
      if (file) {
         bson_copy_to (doc, file);
      }
      mongoc_cursor_destroy (cursor);
   }

   return r;
}

mongoc_gridfs_bucket_t *
mongoc_gridfs_bucket_new (mongoc_database_t *db,
                          const bson_t *opts,
                          const mongoc_read_prefs_t *read_prefs)
{
   mongoc_gridfs_bucket_t *bucket;
   bson_iter_t iter;
   const char *key;
   const char *bucket_name;
   uint32_t bucket_name_len;
   mongoc_write_concern_t *write_concern;
   mongoc_read_concern_t *read_concern;
   int32_t chunk_size;
   char buf[128];

   BSON_ASSERT (db);

   /* Defaults */
   write_concern = NULL;
   read_concern = NULL;
   chunk_size = 255 * 1024;
   bucket_name = "fs";
   bucket_name_len = 2;

   /* Parse the opts */
   if (opts) {
      BSON_ASSERT (bson_iter_init (&iter, opts));
      while (bson_iter_next (&iter)) {
         key = bson_iter_key (&iter);
         if (strcmp (key, "bucketName") == 0) {
            bucket_name = bson_iter_utf8 (&iter, &bucket_name_len);
         } else if (strcmp (key, "chunkSizeBytes") == 0) {
            chunk_size = bson_iter_int32 (&iter);
         } else if (strcmp (key, "writeConcern") == 0) {
            write_concern =
               _mongoc_write_concern_new_from_iter (&iter, NULL /* error */);
            BSON_ASSERT (write_concern);
         } else if (strcmp (key, "readConcern") == 0) {
            read_concern =
               _mongoc_read_concern_new_from_iter (&iter, NULL /* error */);
            BSON_ASSERT (read_concern);
         }
      }
   }

   /* Initialize the bucket fields */
   BSON_ASSERT (bucket_name_len + sizeof (".chunks") < sizeof (buf));

   bucket = (mongoc_gridfs_bucket_t *) bson_malloc0 (sizeof *bucket);

   bson_snprintf (buf, sizeof (buf), "%s.chunks", bucket_name);
   bucket->chunks = mongoc_database_get_collection (db, buf);

   bson_snprintf (buf, sizeof (buf), "%s.files", bucket_name);
   bucket->files = mongoc_database_get_collection (db, buf);

   if (write_concern) {
      mongoc_collection_set_write_concern (bucket->chunks, write_concern);
      mongoc_collection_set_write_concern (bucket->files, write_concern);
      mongoc_write_concern_destroy (write_concern);
   }

   if (read_concern) {
      mongoc_collection_set_read_concern (bucket->chunks, read_concern);
      mongoc_collection_set_read_concern (bucket->files, read_concern);
      mongoc_read_concern_destroy (read_concern);
   }

   if (read_prefs) {
      mongoc_collection_set_read_prefs (bucket->chunks, read_prefs);
      mongoc_collection_set_read_prefs (bucket->files, read_prefs);
   }

   bucket->chunk_size = chunk_size;
   bucket->bucket_name = bson_strdup (bucket_name);

   return bucket;
}


mongoc_stream_t *
mongoc_gridfs_bucket_open_upload_stream_with_id (mongoc_gridfs_bucket_t *bucket,
                                                 const bson_value_t *file_id,
                                                 const char *filename,
                                                 const bson_t *opts,
                                                 bson_error_t *error)
{
   mongoc_gridfs_bucket_file_t *file;
   bson_iter_t iter;
   const char *key;
   bson_t *metadata;
   int32_t chunk_size;
   size_t len;
   uint32_t data_len;
   const uint8_t *data;
   bool r;

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);
   BSON_ASSERT (filename);

   /* Defaults */
   chunk_size = bucket->chunk_size;
   metadata = NULL;

   /* Parse the opts */
   if (opts) {
      r = bson_iter_init (&iter, opts);
      if (!r) {
         bson_set_error (error,
                         MONGOC_ERROR_GRIDFS,
                         MONGOC_ERROR_GRIDFS_PROTOCOL_ERROR,
                         "Error parsing opts.");
         return NULL;
      }

      while (bson_iter_next (&iter)) {
         key = bson_iter_key (&iter);
         if (strcmp (key, "chunkSizeBytes") == 0) {
            chunk_size = bson_iter_int32 (&iter);
         } else if (strcmp (key, "metadata") == 0) {
            bson_iter_document (&iter, &data_len, &data);
            metadata = bson_new_from_data (data, data_len);
         }
      }
   }

   /* Initialize the file's fields */
   len = strlen (filename);

   file = (mongoc_gridfs_bucket_file_t *) bson_malloc0 (sizeof *file);

   file->filename = bson_malloc0 (len + 1);
   bson_strncpy (file->filename, filename, len + 1);

   file->file_id = (bson_value_t *) bson_malloc0 (sizeof *(file->file_id));
   bson_value_copy (file_id, file->file_id);

   file->bucket = bucket;
   file->chunk_size = chunk_size;
   if (metadata) {
      file->metadata = metadata;
   }
   file->buffer = bson_malloc ((size_t) chunk_size);
   file->in_buffer = 0;

   return _mongoc_upload_stream_gridfs_new (file);
}

mongoc_stream_t *
mongoc_gridfs_bucket_open_upload_stream (mongoc_gridfs_bucket_t *bucket,
                                         const char *filename,
                                         const bson_t *opts,
                                         bson_value_t *file_id /* OUT */,
                                         bson_error_t *error)
{
   mongoc_stream_t *stream;
   bson_oid_t object_id;
   bson_value_t val;

   BSON_ASSERT (bucket);
   BSON_ASSERT (filename);

   /* Create an objectId to use as the file's id */
   bson_oid_init (&object_id, bson_context_get_default ());
   val.value_type = BSON_TYPE_OID;
   val.value.v_oid = object_id;

   stream = mongoc_gridfs_bucket_open_upload_stream_with_id (
      bucket, &val, filename, opts, error);

   if (!stream) {
      return NULL;
   }

   if (file_id) {
      bson_value_copy (&val, file_id);
   }

   return stream;
}

bool
mongoc_gridfs_bucket_upload_from_stream_with_id (mongoc_gridfs_bucket_t *bucket,
                                                 const bson_value_t *file_id,
                                                 const char *filename,
                                                 mongoc_stream_t *source,
                                                 const bson_t *opts,
                                                 bson_error_t *error)
{
   mongoc_stream_t *upload_stream;
   ssize_t bytes_read;
   ssize_t bytes_written;
   char buf[512];

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);
   BSON_ASSERT (filename);
   BSON_ASSERT (source);

   upload_stream = mongoc_gridfs_bucket_open_upload_stream_with_id (
      bucket, file_id, filename, opts, error);

   if (!upload_stream) {
      return false;
   }

   while ((bytes_read = mongoc_stream_read (source, buf, 512, 1, 0)) > 0) {
      bytes_written = mongoc_stream_write (upload_stream, buf, bytes_read, 0);
      if (bytes_written < 0) {
         /* Error should already be set */
         mongoc_gridfs_bucket_abort_upload (upload_stream);
         mongoc_stream_destroy (upload_stream);
         return false;
      }
   }

   if (bytes_read < 0) {
      mongoc_gridfs_bucket_abort_upload (upload_stream);
      bson_set_error (error,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_PROTOCOL_ERROR,
                      "Error occurred on the provided stream.");
      mongoc_stream_destroy (upload_stream);
      return false;
   } else {
      /* Destroying the stream first closes it */
      mongoc_stream_destroy (upload_stream);
      return true;
   }
}

bool
mongoc_gridfs_bucket_upload_from_stream (mongoc_gridfs_bucket_t *bucket,
                                         const char *filename,
                                         mongoc_stream_t *source,
                                         const bson_t *opts,
                                         bson_value_t *file_id /* OUT */,
                                         bson_error_t *error)
{
   bool r;
   bson_oid_t object_id;
   bson_value_t val;

   BSON_ASSERT (bucket);
   BSON_ASSERT (filename);
   BSON_ASSERT (source);

   /* Create an objectId to use as the file's id */
   bson_oid_init (&object_id, bson_context_get_default ());
   val.value_type = BSON_TYPE_OID;
   val.value.v_oid = object_id;

   r = mongoc_gridfs_bucket_upload_from_stream_with_id (
      bucket, &val, filename, source, opts, error);

   if (!r) {
      return false;
   }

   if (file_id) {
      bson_value_copy (&val, file_id);
   }

   return true;
}


mongoc_stream_t *
mongoc_gridfs_bucket_open_download_stream (mongoc_gridfs_bucket_t *bucket,
                                           const bson_value_t *file_id,
                                           bson_error_t *error)
{
   mongoc_gridfs_bucket_file_t *file;
   bson_t file_doc;
   const char *key;
   bson_iter_t iter;
   uint32_t data_len;
   const uint8_t *data;
   bool r;

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);

   r = _mongoc_gridfs_find_file_with_id (bucket, file_id, &file_doc, error);
   if (!r) {
      /* Error should already be set on the bucket. */
      return NULL;
   }

   file = (mongoc_gridfs_bucket_file_t *) bson_malloc0 (sizeof *file);

   bson_iter_init (&iter, &file_doc);

   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);
      if (strcmp (key, "length") == 0) {
         file->length = bson_iter_as_int64 (&iter);
      } else if (strcmp (key, "chunkSize") == 0) {
         file->chunk_size = bson_iter_int32 (&iter);
      } else if (strcmp (key, "filename") == 0) {
         file->filename = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (strcmp (key, "metadata") == 0) {
         bson_iter_document (&iter, &data_len, &data);
         file->metadata = bson_new_from_data (data, data_len);
      }
   }

   bson_destroy (&file_doc);

   file->file_id = (bson_value_t *) bson_malloc0 (sizeof *(file->file_id));
   bson_value_copy (file_id, file->file_id);
   file->bucket = bucket;
   file->buffer = bson_malloc0 ((size_t) file->chunk_size);

   BSON_ASSERT (file->file_id);

   return _mongoc_download_stream_gridfs_new (file);
}

bool
mongoc_gridfs_bucket_download_to_stream (mongoc_gridfs_bucket_t *bucket,
                                         const bson_value_t *file_id,
                                         mongoc_stream_t *destination,
                                         bson_error_t *error)
{
   mongoc_stream_t *download_stream;
   ssize_t bytes_read;
   ssize_t bytes_written;
   char buf[512];

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);
   BSON_ASSERT (destination);

   /* Make the download stream */
   download_stream =
      mongoc_gridfs_bucket_open_download_stream (bucket, file_id, error);

   while ((bytes_read = mongoc_stream_read (download_stream, buf, 256, 1, 0)) >
          0) {
      bytes_written = mongoc_stream_write (destination, buf, bytes_read, 0);
      if (bytes_written < 0) {
         mongoc_stream_destroy (download_stream);
         bson_set_error (error,
                         MONGOC_ERROR_GRIDFS,
                         MONGOC_ERROR_GRIDFS_PROTOCOL_ERROR,
                         "Error occurred on the provided stream.");
         return false;
      }
   }

   /* Destroying the stream first closes it */
   mongoc_stream_destroy (download_stream);

   return bytes_read != -1;
}

bool
mongoc_gridfs_bucket_delete_by_id (mongoc_gridfs_bucket_t *bucket,
                                   const bson_value_t *file_id,
                                   bson_error_t *error)
{
   bson_t files_selector;
   bson_t chunks_selector;
   bool r;

   BSON_ASSERT (bucket);
   BSON_ASSERT (file_id);

   r = _mongoc_gridfs_find_file_with_id (bucket, file_id, NULL, error);
   if (!r) {
      return false;
   }

   bson_init (&files_selector);

   BSON_APPEND_VALUE (&files_selector, "_id", file_id);

   r = mongoc_collection_delete_one (
      bucket->files, &files_selector, NULL, NULL, error);
   bson_destroy (&files_selector);
   if (!r) {
      return false;
   }

   bson_init (&chunks_selector);

   BSON_APPEND_VALUE (&chunks_selector, "files_id", file_id);

   r = mongoc_collection_delete_many (
      bucket->chunks, &chunks_selector, NULL, NULL, error);
   bson_destroy (&chunks_selector);
   if (!r) {
      return false;
   }

   return true;
}

mongoc_cursor_t *
mongoc_gridfs_bucket_find (mongoc_gridfs_bucket_t *bucket,
                           const bson_t *filter,
                           const bson_t *opts)
{
   mongoc_cursor_t *result;
   BSON_ASSERT (bucket);
   BSON_ASSERT (filter);

   if (opts) {
      bson_t *exclude_opts = bson_new ();
      bson_copy_to_excluding_noinit (opts, exclude_opts, "sessionId", NULL);
      result = mongoc_collection_find_with_opts (
         bucket->files, filter, exclude_opts, NULL);
      bson_destroy (exclude_opts);
      bson_free (exclude_opts);
      return result;
   } else {
      return mongoc_collection_find_with_opts (
         bucket->files, filter, NULL, NULL);
   }
}

bool
mongoc_gridfs_bucket_stream_error (mongoc_stream_t *stream, bson_error_t *error)
{
   bson_error_t *stream_err;
   BSON_ASSERT (stream);
   BSON_ASSERT (error);


   if (stream->type == MONGOC_STREAM_GRIDFS_UPLOAD) {
      stream_err = &((mongoc_gridfs_upload_stream_t *) stream)->file->err;
   } else if (stream->type == MONGOC_STREAM_GRIDFS_DOWNLOAD) {
      stream_err = &((mongoc_gridfs_download_stream_t *) stream)->file->err;
   } else {
      return false;
   }

   if (stream_err->code) {
      memcpy (error, stream_err, sizeof (*stream_err));
      return true;
   } else {
      return false;
   }
}

void
mongoc_gridfs_bucket_destroy (mongoc_gridfs_bucket_t *bucket)
{
   if (bucket) {
      mongoc_collection_destroy (bucket->chunks);
      mongoc_collection_destroy (bucket->files);
      bson_free (bucket->bucket_name);
      bson_free (bucket);
   }
}

bool
mongoc_gridfs_bucket_abort_upload (mongoc_stream_t *stream)
{
   mongoc_gridfs_bucket_file_t *file;
   bson_t *chunks_selector;
   bool r;

   BSON_ASSERT (stream);

   if (stream->type != MONGOC_STREAM_GRIDFS_UPLOAD) {
      /* No way to send the user an error. Just return NULL */
      return false;
   }

   file = ((mongoc_gridfs_upload_stream_t *) stream)->file;

   /* Pretend we've already saved. This way we won't add an entry to the files
    * collection when the stream is closed */
   file->saved = true;

   chunks_selector = bson_new ();
   BSON_APPEND_VALUE (chunks_selector, "files_id", file->file_id);

   r = mongoc_collection_delete_many (
      file->bucket->chunks, chunks_selector, NULL, NULL, &file->err);
   bson_destroy (chunks_selector);
   return r;
}