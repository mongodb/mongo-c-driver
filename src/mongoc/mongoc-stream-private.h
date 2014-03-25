/*
 * Copyright 2013-2014 MongoDB, Inc.
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


#ifndef MONGOC_STREAM_PRIVATE_H
#define MONGOC_STREAM_PRIVATE_H


#include "mongoc-iovec.h"
#include "mongoc-stream.h"


BSON_BEGIN_DECLS


#define MONGOC_STREAM_SOCKET   1
#define MONGOC_STREAM_FILE     2
#define MONGOC_STREAM_BUFFERED 3
#define MONGOC_STREAM_GRIDFS   4
#define MONGOC_STREAM_TLS      5


struct _mongoc_stream_t
{
   int              type;
   void             (*destroy)         (mongoc_stream_t *stream);
   int              (*close)           (mongoc_stream_t *stream);
   int              (*flush)           (mongoc_stream_t *stream);
   ssize_t          (*writev)          (mongoc_stream_t *stream,
                                        mongoc_iovec_t  *iov,
                                        size_t           iovcnt,
                                        int32_t          timeout_msec);
   ssize_t          (*readv)           (mongoc_stream_t *stream,
                                        mongoc_iovec_t  *iov,
                                        size_t           iovcnt,
                                        size_t           min_bytes,
                                        int32_t          timeout_msec);
   int              (*cork)            (mongoc_stream_t *stream);
   int              (*uncork)          (mongoc_stream_t *stream);
   int              (*setsockopt)      (mongoc_stream_t *stream,
                                        int              level,
                                        int              optname,
                                        void            *optval,
                                        socklen_t        optlen);
   mongoc_stream_t *(*get_base_stream) (mongoc_stream_t *stream);

};


BSON_END_DECLS


#endif /* MONGOC_STREAM_PRIVATE_H */
