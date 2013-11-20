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
#define MONGOC_LOG_DOMAIN "gridfs"

#include "mongoc-collection.h"
#include "mongoc-collection-private.h"
#include "mongoc-index.h"
#include "mongoc-gridfs.h"
#include "mongoc-gridfs-private.h"
#include "mongoc-gridfs-file.h"
#include "mongoc-gridfs-file-private.h"
#include "mongoc-gridfs-file-list.h"
#include "mongoc-gridfs-file-list-private.h"
#include "mongoc-client.h"
#include "mongoc-trace.h"

#define MONGOC_GRIDFS_STREAM_CHUNK 4096


/**
 * _mongoc_gridfs_ensure_index:
 *
 * ensure gridfs indexes
 *
 * Ensure fast searches for chunks via [ files_id, n ]
 * Ensure fast searches for files via [ filename ]
 */
static bson_bool_t
_mongoc_gridfs_ensure_index (mongoc_gridfs_t *gridfs,
                             bson_error_t    *error)
{
   bson_t keys;
   mongoc_index_opt_t opt;
   bson_bool_t r;

   ENTRY;

   bson_init (&keys);

   bson_append_int32 (&keys, "files_id", -1, 1);
   bson_append_int32 (&keys, "n", -1, 1);

   mongoc_index_opt_init (&opt);
   opt.unique = 1;

   r = mongoc_collection_ensure_index (gridfs->chunks, &keys, &opt, error);

   bson_destroy (&keys);

   if (!r) { RETURN (r); }

   bson_init (&keys);

   bson_append_int32 (&keys, "filename", -1, 1);
   opt.unique = 0;

   r = mongoc_collection_ensure_index (gridfs->chunks, &keys, &opt, error);

   bson_destroy (&keys);

   if (!r) { RETURN (r); }

   RETURN (1);
}


mongoc_gridfs_t *
_mongoc_gridfs_new (mongoc_client_t *client,
                    const char      *db,
                    const char      *prefix,
                    bson_error_t    *error)
{
   mongoc_gridfs_t *gridfs;
   char buf[128];
   bson_uint32_t prefix_len;
   bson_bool_t r;

   ENTRY;

   prefix_len = strlen (prefix);

   BSON_ASSERT (client);
   BSON_ASSERT (db);

   if (!prefix) {
      prefix = "fs";
   }

   /* make sure prefix is short enough to bucket the chunks and files
    * collections
    */
   BSON_ASSERT (prefix_len + sizeof (".chunks") < sizeof (buf));

   gridfs = bson_malloc0 (sizeof *gridfs);

   gridfs->client = client;

   sprintf (buf, "%s.chunks", prefix);
   gridfs->chunks = _mongoc_collection_new (client, db, buf, NULL, NULL);

   sprintf (buf, "%s.files", prefix);
   gridfs->files = _mongoc_collection_new (client, db, buf, NULL, NULL);

   r = _mongoc_gridfs_ensure_index (gridfs, error);

   if (!r) { return NULL; }

   RETURN (gridfs);
}


bson_bool_t
mongoc_gridfs_drop (mongoc_gridfs_t *gridfs,
                    bson_error_t    *error)
{
   bson_bool_t r;

   ENTRY;

   r = mongoc_collection_drop (gridfs->files, error);
   if (!r) {
      RETURN (0);
   }

   r = mongoc_collection_drop (gridfs->chunks, error);
   if (!r) {
      RETURN (0);
   }

   RETURN (1);
}


void
mongoc_gridfs_destroy (mongoc_gridfs_t *gridfs)
{
   ENTRY;

   BSON_ASSERT (gridfs);

   mongoc_collection_destroy (gridfs->files);
   mongoc_collection_destroy (gridfs->chunks);

   bson_free (gridfs);

   EXIT;
}


/** find all matching gridfs files */
mongoc_gridfs_file_list_t *
mongoc_gridfs_find (mongoc_gridfs_t *gridfs,
                    const bson_t    *query)
{
   return mongoc_gridfs_file_list_new (gridfs, query, 0);
}


/** find a single gridfs file */
mongoc_gridfs_file_t *
mongoc_gridfs_find_one (mongoc_gridfs_t *gridfs,
                        const bson_t    *query)
{
   mongoc_gridfs_file_list_t *list;
   mongoc_gridfs_file_t *file;

   ENTRY;

   list = mongoc_gridfs_file_list_new (gridfs, query, 1);

   file = mongoc_gridfs_file_list_next (list);

   mongoc_gridfs_file_list_destroy (list);

   RETURN (file);
}


/** find a single gridfs file by filename */
mongoc_gridfs_file_t *
mongoc_gridfs_find_one_by_filename (mongoc_gridfs_t *gridfs,
                                    const char      *filename)
{
   mongoc_gridfs_file_t *file;

   bson_t query;

   bson_init (&query);

   bson_append_utf8 (&query, "filename", -1, filename, -1);

   file = mongoc_gridfs_find_one (gridfs, &query);

   bson_destroy (&query);

   return file;
}


/** create a gridfs file from a stream
 *
 * The stream is fully consumed in creating the file
 */
mongoc_gridfs_file_t *
mongoc_gridfs_create_file_from_stream (mongoc_gridfs_t          *gridfs,
                                       mongoc_stream_t          *stream,
                                       mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;
   ssize_t r;
   bson_uint8_t buf[MONGOC_GRIDFS_STREAM_CHUNK];
   struct iovec iov = { buf, 0 };

   ENTRY;

   BSON_ASSERT (gridfs);
   BSON_ASSERT (stream);

   file = mongoc_gridfs_file_new (gridfs, opt);

   for (;; ) {
      r = mongoc_stream_read (stream, iov.iov_base, MONGOC_GRIDFS_STREAM_CHUNK,
                              -1, 0);

      if (r > 0) {
         iov.iov_len = r;
         mongoc_gridfs_file_writev (file, &iov, 1, 0);
      } else if (r == 0) {
         break;
      } else {
         mongoc_gridfs_file_destroy (file);
         RETURN (NULL);
      }
   }

   mongoc_stream_destroy (stream);

   mongoc_gridfs_file_seek (file, 0, SEEK_SET);

   RETURN (file);
}


/** create an empty gridfs file */
mongoc_gridfs_file_t *
mongoc_gridfs_create_file (mongoc_gridfs_t          *gridfs,
                           mongoc_gridfs_file_opt_t *opt)
{
   mongoc_gridfs_file_t *file;

   ENTRY;

   BSON_ASSERT (gridfs);

   file = mongoc_gridfs_file_new (gridfs, opt);

   RETURN (file);
}
