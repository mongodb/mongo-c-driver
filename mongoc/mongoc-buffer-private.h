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


#ifndef MONGOC_BUFFER_PRIVATE_H
#define MONGOC_BUFFER_PRIVATE_H


#include <bson.h>

#include "mongoc-stream.h"


BSON_BEGIN_DECLS


typedef struct _mongoc_buffer_t mongoc_buffer_t;


struct _mongoc_buffer_t
{
   bson_uint8_t       *data;
   size_t              datalen;
   size_t              off;
   size_t              len;
   bson_realloc_func   realloc_func;
};


void
mongoc_buffer_init (mongoc_buffer_t   *buffer,
                    bson_uint8_t      *buf,
                    size_t             buflen,
                    bson_realloc_func  realloc_func);


bson_bool_t
mongoc_buffer_fill (mongoc_buffer_t *buffer,
                    mongoc_stream_t *stream,
                    size_t           minsize,
                    bson_error_t    *error);


void
mongoc_buffer_destroy (mongoc_buffer_t *buffer);


ssize_t
mongoc_buffer_read (mongoc_buffer_t *buffer,
                    struct iovec    *iov,
                    size_t           iovcnt);


BSON_END_DECLS


#endif /* MONGOC_BUFFER_PRIVATE_H */
