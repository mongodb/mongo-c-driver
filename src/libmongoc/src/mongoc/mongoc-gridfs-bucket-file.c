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
#include "mongoc-gridfs-bucket-file-private.h"
#include "mongoc-gridfs-bucket-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-stream-gridfs-download-private.h"
#include "mongoc-stream-gridfs-upload-private.h"

/* Returns the minimum of two numbers */
static size_t
_mongoc_min (const size_t a, const size_t b)
{
   return a < b ? a : b;
}

/*
 * Creates an index in the given collection if it doesn't already exist.
 *
 * Returns true if the index was already present or was successfully created.
 * Returns false if an error occurred while trying to create the index.
 */
static bool
_mongoc_create_index_if_not_present (mongoc_collection_t *col,
                                     const bson_t *index,
                                     bool unique)
{
   mongoc_cursor_t *cursor;
   bool index_exists;
   bool r;
   const bson_t *doc;
   bson_iter_t iter;
   bson_t *inner_doc;
   char *index_name;
   bson_t index_command;
   uint32_t data_len;
   const uint8_t *data;

   BSON_ASSERT (col);
   BSON_ASSERT (index);

   cursor = mongoc_collection_find_indexes_with_opts (col, NULL);

   index_exists = false;

   while (mongoc_cursor_next (cursor, &doc)) {
      r = bson_iter_init_find (&iter, doc, "key");
      if (!r) {
         continue;
      }
      bson_iter_document (&iter, &data_len, &data);
      inner_doc = bson_new_from_data (data, data_len);
      if (bson_compare (inner_doc, index) == 0) {
         index_exists = true;
      }
      bson_destroy (inner_doc);
   }

   mongoc_cursor_destroy (cursor);

   if (!index_exists) {
      index_name = mongoc_collection_keys_to_index_string (index);
      bson_init (&index_command);
      BCON_APPEND (&index_command,
                   "createIndexes",
                   BCON_UTF8 (mongoc_collection_get_name (col)),
                   "indexes",
                   "[",
                   "{",
                   "key",
                   BCON_DOCUMENT (index),
                   "name",
                   BCON_UTF8 (index_name),
                   "unique",
                   BCON_BOOL (unique),
                   "}",
                   "]");

      r = mongoc_collection_write_command_with_opts (
         col, &index_command, NULL, NULL, NULL);
      bson_destroy (&index_command);
      bson_free (index_name);
      if (!r) {
         return false;
      }
   }

   return true;
}

/*
 * Creates the indexes needed for gridFS
 *
 * Returns true if creating the indexes was successful, otherwise returns false.
 */
static bool
_mongoc_gridfs_bucket_set_indexes (mongoc_gridfs_bucket_t *bucket)
{
   mongoc_read_prefs_t *prefs;
   bson_t filter;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_t *files_index;
   bson_t *chunks_index;
   bool r;

   bson_init (&filter);

   /* Check to see if there already exists a document in the files collection */
   prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);
   cursor =
      mongoc_collection_find_with_opts (bucket->files, &filter, NULL, prefs);
   bson_destroy (&filter);

   r = mongoc_cursor_next (cursor, &doc);

   mongoc_read_prefs_destroy (prefs);
   mongoc_cursor_destroy (cursor);

   if (r) {
      /* We are done here if files already exist */
      return true;
   }

   /* Create files index */
   files_index = bson_new ();
   BSON_APPEND_INT32 (files_index, "filename", 1);
   BSON_APPEND_INT32 (files_index, "uploadDate", 1);
   r = _mongoc_create_index_if_not_present (bucket->files, files_index, false);
   bson_destroy (files_index);
   if (!r) {
      return false;
   }

   /* Create unique chunks index */
   chunks_index = bson_new ();
   BSON_APPEND_INT32 (chunks_index, "files_id", 1);
   BSON_APPEND_INT32 (chunks_index, "n", 1);
   r = _mongoc_create_index_if_not_present (bucket->chunks, chunks_index, true);
   bson_destroy (chunks_index);
   if (!r) {
      return false;
   }

   return true;
}

/*
 * Writes a chunk from the buffer into the chunks collection.
 *
 * Returns true if the chunk was successfully written. Otherwise, it returns
 * false and sets an error on the bucket.
 */
static bool
_mongoc_gridfs_bucket_write_chunk (mongoc_gridfs_bucket_file_t *file)
{
   bson_t chunk;
   bool r;

   BSON_ASSERT (file);

   bson_init (&chunk);

   BSON_APPEND_INT32 (&chunk, "n", file->curr_chunk);
   BSON_APPEND_VALUE (&chunk, "files_id", file->file_id);
   BSON_APPEND_BINARY (&chunk,
                       "data",
                       BSON_SUBTYPE_BINARY,
                       file->buffer,
                       (uint32_t) file->in_buffer);


   r = mongoc_collection_insert_one (file->bucket->chunks,
                                     &chunk,
                                     NULL /* opts */,
                                     NULL /* reply */,
                                     &file->err);
   bson_destroy (&chunk);
   if (!r) {
      return false;
   }

   file->curr_chunk++;
   file->in_buffer = 0;
   return true;
}

/*
 * Initializes the cursor at file->cursor for the given file.
 */
static void
_mongoc_gridfs_bucket_init_cursor (mongoc_gridfs_bucket_file_t *file)
{
   bson_t filter;
   bson_t opts;
   bson_t sort;

   BSON_ASSERT (file);

   bson_init (&filter);
   bson_init (&opts);
   bson_init (&sort);

   BSON_APPEND_VALUE (&filter, "files_id", file->file_id);
   BSON_APPEND_INT32 (&sort, "n", 1);
   BSON_APPEND_DOCUMENT (&opts, "sort", &sort);

   file->cursor = mongoc_collection_find_with_opts (
      file->bucket->chunks, &filter, &opts, NULL);

   bson_destroy (&filter);
   bson_destroy (&opts);
   bson_destroy (&sort);
}

/*
 * Reads a chunk from the server and places it into the file's buffer
 *
 * Returns true if the buffer has been filled with any available data.
 * Otherwise, returns false and sets the error on the bucket
 */
static bool
_mongoc_gridfs_bucket_read_chunk (mongoc_gridfs_bucket_file_t *file)
{
   const bson_t *next;
   bool r;
   bson_iter_t iter;
   int32_t n;
   const uint8_t *data;
   uint32_t data_len;
   int64_t total_chunks;
   int64_t last_chunk_size;

   BSON_ASSERT (file);

   if (file->length == 0) {
      /* This file has zero length */
      file->in_buffer = 0;
      file->finished = true;
      return true;
   }

   /* Calculate the total number of chunks for this file */
   total_chunks = (file->length / file->chunk_size);
   if (file->length % file->chunk_size != 0) {
      total_chunks++;
   }

   if (file->curr_chunk == total_chunks) {
      /* All chunks have been read! */
      file->in_buffer = 0;
      file->finished = true;
      return true;
   }

   if (file->cursor == NULL) {
      _mongoc_gridfs_bucket_init_cursor (file);
   }

   r = mongoc_cursor_next (file->cursor, &next);

   if (mongoc_cursor_error (file->cursor, &file->err)) {
      return false;
   }

   if (!r) {
      bson_set_error (&file->err,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_CHUNK_MISSING,
                      "Missing a chunk.");
      return false;
   }

   r = bson_iter_init_find (&iter, next, "n");
   if (!r) {
      bson_set_error (&file->err,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_CORRUPT,
                      "Chunk missing a required field.");
      return false;
   }

   n = bson_iter_int32 (&iter);

   if (n != file->curr_chunk) {
      bson_set_error (&file->err,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_CHUNK_MISSING,
                      "Missing a chunk.");
      return false;
   }

   r = bson_iter_init_find (&iter, next, "data");
   if (!r) {
      bson_set_error (&file->err,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_CORRUPT,
                      "Chunk missing a required field.");
      return false;
   }

   bson_iter_binary (&iter, NULL, &data_len, &data);

   /* Assert that the data is the correct length */
   if (file->curr_chunk != total_chunks - 1) {
      if (data_len != file->chunk_size) {
         bson_set_error (&file->err,
                         MONGOC_ERROR_GRIDFS,
                         MONGOC_ERROR_GRIDFS_CORRUPT,
                         "Chunk is the wrong size");
         return false;
      }
   } else {
      last_chunk_size = file->length - ((total_chunks - 1) * file->chunk_size);
      if (data_len != last_chunk_size) {
         bson_set_error (&file->err,
                         MONGOC_ERROR_GRIDFS,
                         MONGOC_ERROR_GRIDFS_CORRUPT,
                         "Chunk is the wrong size");
         return false;
      }
   }

   memcpy (file->buffer, data, data_len);
   file->in_buffer = data_len;
   file->bytes_read = 0;
   file->curr_chunk++;

   return true;
}

ssize_t
_mongoc_gridfs_bucket_file_writev (mongoc_gridfs_bucket_file_t *file,
                                   const mongoc_iovec_t *iov,
                                   size_t iovcnt,
                                   uint32_t timeout_msec)
{
   uint32_t total;
   size_t bytes_available;
   size_t space_available;
   int32_t written_this_iov;
   size_t to_write;
   size_t i;
   bool r;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   total = 0;

   if (file->err.code) {
      return -1;
   }

   if (file->saved) {
      bson_set_error (&file->err,
                      MONGOC_ERROR_GRIDFS,
                      MONGOC_ERROR_GRIDFS_PROTOCOL_ERROR,
                      "Trying to write on a closed stream.");
      return -1;
   }

   if (!file->bucket->indexed) {
      r = _mongoc_gridfs_bucket_set_indexes (file->bucket);
      if (!r) {
         bson_set_error (&file->err,
                         MONGOC_ERROR_GRIDFS,
                         MONGOC_ERROR_GRIDFS_PROTOCOL_ERROR,
                         "Creating required indexes failed.");
         return -1;
      } else {
         file->bucket->indexed = true;
      }
   }

   for (i = 0; i < iovcnt; i++) {
      written_this_iov = 0;

      while (written_this_iov < iov[i].iov_len) {
         bytes_available = iov[i].iov_len - written_this_iov;
         space_available = file->chunk_size - file->in_buffer;
         to_write = _mongoc_min (bytes_available, space_available);
         memcpy (file->buffer + file->in_buffer,
                 iov[i].iov_base + written_this_iov,
                 to_write);
         file->in_buffer += to_write;
         written_this_iov += to_write;
         total += to_write;
         if (file->in_buffer == file->chunk_size) {
            /* Buffer is filled, write the chunk */
            _mongoc_gridfs_bucket_write_chunk (file);
         }
      }
   }

   return total;
}

ssize_t
_mongoc_gridfs_bucket_file_readv (mongoc_gridfs_bucket_file_t *file,
                                  mongoc_iovec_t *iov,
                                  size_t iovcnt,
                                  size_t min_bytes,
                                  uint32_t timeout_msec)
{
   uint32_t total;
   size_t bytes_available;
   size_t space_available;
   int32_t read_this_iov;
   size_t to_read;
   bool r;
   size_t i;

   BSON_ASSERT (file);
   BSON_ASSERT (iov);
   BSON_ASSERT (iovcnt);

   total = 0;

   if (file->err.code) {
      return -1;
   }

   if (file->finished) {
      return 0;
   }

   for (i = 0; i < iovcnt; i++) {
      read_this_iov = 0;

      while (read_this_iov < iov[i].iov_len) {
         bytes_available = file->in_buffer - file->bytes_read;
         space_available = iov[i].iov_len - read_this_iov;
         to_read = _mongoc_min (bytes_available, space_available);
         memcpy (iov[i].iov_base + read_this_iov,
                 file->buffer + file->bytes_read,
                 to_read);
         file->bytes_read += to_read;
         read_this_iov += to_read;
         total += to_read;
         if (file->bytes_read == file->in_buffer) {
            /* Everything in the current chunk has been read, so read a new
             * chunk */
            r = _mongoc_gridfs_bucket_read_chunk (file);
            if (!r) {
               /* an error occured while reading the chunk */
               return -1;
            }
            if (file->finished) {
               /* There's nothing left to read */
               RETURN (total);
            }
         }
      }
   }

   RETURN (total);
}


/*
 * Saves the file to the files collection in gridFS. This locks the file into
 * gridFS, and no more chunks are allowed to be written.
 */
int
_mongoc_gridfs_bucket_file_save (mongoc_gridfs_bucket_file_t *file)
{
   bson_t new_doc;
   int64_t length;
   bool r;

   BSON_ASSERT (file);

   if (file->saved) {
      /* This file has already been saved! */
      return 0;
   }

   if (file->err.code) {
      return -1;
   }
   
   length = file->curr_chunk * file->chunk_size;

   if (file->in_buffer != 0) {
      length += file->in_buffer;
      _mongoc_gridfs_bucket_write_chunk (file);
   }

   file->length = length;

   bson_init (&new_doc);
   BSON_APPEND_VALUE (&new_doc, "_id", file->file_id);
   BSON_APPEND_INT64 (&new_doc, "length", file->length);
   BSON_APPEND_INT32 (&new_doc, "chunkSize", file->chunk_size);
   BSON_APPEND_DATE_TIME (&new_doc, "uploadDate", bson_get_monotonic_time ());
   BSON_APPEND_UTF8 (&new_doc, "filename", file->filename);
   if (file->metadata) {
      BSON_APPEND_DOCUMENT (&new_doc, "metadata", file->metadata);
   }

   r = mongoc_collection_insert_one (
      file->bucket->files, &new_doc, NULL, NULL, &file->err);
   bson_destroy (&new_doc);
   file->saved = r;
   return 1;
}

void
_mongoc_gridfs_bucket_file_destroy (mongoc_gridfs_bucket_file_t *file)
{
   /* Do cleanup! */
   if (file) {
      bson_value_destroy (file->file_id);
      bson_free (file->file_id);
      bson_destroy (file->metadata);
      mongoc_cursor_destroy (file->cursor);
      bson_free (file->buffer);
      bson_free (file->filename);
      bson_free (file);
   }
}
