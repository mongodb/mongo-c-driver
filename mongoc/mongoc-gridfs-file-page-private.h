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


#ifndef MONGOC_GRIDFS_FILE_PAGE_PRIVATE_H
#define MONGOC_GRIDFS_FILE_PAGE_PRIVATE_H


#include <bson.h>

#include "mongoc-gridfs-file.h"


BSON_BEGIN_DECLS


struct _mongoc_gridfs_file_page
{
   const bson_uint8_t *read_buf;
   bson_uint8_t       *buf;
   bson_uint32_t       len;
   bson_uint32_t       chunk_size;
   bson_uint32_t       offset;
};

mongoc_gridfs_file_page_t *
_mongoc_gridfs_file_page_new (const bson_uint8_t *data,
                              bson_uint32_t       len,
                              bson_uint32_t       chunk_size)
   BSON_GNUC_INTERNAL;

void
_mongoc_gridfs_file_page_destroy (mongoc_gridfs_file_page_t *page)
   BSON_GNUC_INTERNAL;

bson_bool_t
_mongoc_gridfs_file_page_seek (mongoc_gridfs_file_page_t *page,
                               bson_uint32_t              offset)
   BSON_GNUC_INTERNAL;

bson_int32_t
_mongoc_gridfs_file_page_read (mongoc_gridfs_file_page_t *page,
                               void                      *dst,
                               bson_uint32_t              len)
   BSON_GNUC_INTERNAL;

bson_int32_t
_mongoc_gridfs_file_page_write (mongoc_gridfs_file_page_t *page,
                                const void                *src,
                                bson_uint32_t              len)
   BSON_GNUC_INTERNAL;

bson_uint32_t
_mongoc_gridfs_file_page_tell (mongoc_gridfs_file_page_t *page)
   BSON_GNUC_INTERNAL;

const bson_uint8_t *
_mongoc_gridfs_file_page_get_data (mongoc_gridfs_file_page_t *page)
   BSON_GNUC_INTERNAL;

bson_uint32_t
_mongoc_gridfs_file_page_get_len (mongoc_gridfs_file_page_t *page)
   BSON_GNUC_INTERNAL;

bson_bool_t
_mongoc_gridfs_file_page_is_dirty (mongoc_gridfs_file_page_t *page)
   BSON_GNUC_INTERNAL;


BSON_END_DECLS


#endif /* MONGOC_GRIDFS_FILE_PAGE_PRIVATE_H */
