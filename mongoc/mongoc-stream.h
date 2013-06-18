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


#ifndef MONGOC_STREAM_H
#define MONGOC_STREAM_H


#include <bson.h>
#include <sys/uio.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_stream_t mongoc_stream_t;


struct _mongoc_stream_t
{
   void    (*destroy) (mongoc_stream_t *stream);
   int     (*close)   (mongoc_stream_t *stream);
   int     (*flush)   (mongoc_stream_t *stream);
   ssize_t (*writev)  (mongoc_stream_t *stream,
                       struct iovec    *iov,
                       size_t           iovcnt);
   ssize_t (*readv)   (mongoc_stream_t *stream,
                       struct iovec    *iov,
                       size_t           iovcnt);
   int     (*cork)    (mongoc_stream_t *stream);
   int     (*uncork)  (mongoc_stream_t *stream);
};


int              mongoc_stream_close         (mongoc_stream_t *stream);
int              mongoc_stream_cork          (mongoc_stream_t *stream);
int              mongoc_stream_uncork        (mongoc_stream_t *stream);
void             mongoc_stream_destroy       (mongoc_stream_t *stream);
int              mongoc_stream_flush         (mongoc_stream_t *stream);
mongoc_stream_t *mongoc_stream_new_from_unix (int              fd);
mongoc_stream_t *mongoc_stream_buffered_new  (mongoc_stream_t *base_stream);
ssize_t          mongoc_stream_writev        (mongoc_stream_t *stream,
                                              struct iovec    *iov,
                                              size_t           iovcnt);
ssize_t          mongoc_stream_readv         (mongoc_stream_t *stream,
                                              struct iovec    *iov,
                                              size_t           iovcnt);
bson_t          *mongoc_stream_ismaster      (mongoc_stream_t *stream,
                                              bson_error_t    *error);


BSON_END_DECLS


#endif /* MONGOC_STREAM_H */
