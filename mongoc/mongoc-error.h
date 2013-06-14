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


#ifndef MONGOC_ERRORS_H
#define MONGOC_ERRORS_H


#include <bson.h>


BSON_BEGIN_DECLS


typedef enum
{
   MONGOC_ERROR_CLIENT = 1,
   MONGOC_ERROR_STREAM,
   MONGOC_ERROR_PROTOCOL,
   MONGOC_ERROR_CURSOR,
   MONGOC_ERROR_QUERY,
} mongoc_error_domain_t;


typedef enum
{
   MONGOC_ERROR_STREAM_INVALID_TYPE = 1,
   MONGOC_ERROR_STREAM_INVALID_STATE,
   MONGOC_ERROR_STREAM_NAME_RESOLUTION,
   MONGOC_ERROR_STREAM_SOCKET,
   MONGOC_ERROR_STREAM_CONNECT,
   MONGOC_ERROR_STREAM_NOT_ESTABLISHED,

   MONGOC_ERROR_CLIENT_NOT_READY,
   MONGOC_ERROR_CLIENT_TOO_BIG,
   MONGOC_ERROR_CLIENT_TOO_SMALL,

   MONGOC_ERROR_PROTOCOL_INVALID_REPLY,

   MONGOC_ERROR_CURSOR_INVALID_CURSOR,

   MONGOC_ERROR_QUERY_FAILURE,
} mongoc_error_code_t;


BSON_END_DECLS


#endif /* MONGOC_ERRORS_H */
