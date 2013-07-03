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


#ifndef MONGOC_WRITE_CONCERN_PRIVATE_H
#define MONGOC_WRITE_CONCERN_PRIVATE_H


#include <bson.h>


BSON_BEGIN_DECLS


struct _mongoc_write_concern_t
{
   bson_bool_t  fsync_;
   bson_bool_t  journal;
   bson_int32_t w;
   bson_int32_t wtimeout;
   bson_t       tags;
   bson_t       compiled;
};


const bson_t *mongoc_write_concern_compile (mongoc_write_concern_t *write_concern);


BSON_END_DECLS


#endif /* MONGOC_WRITE_CONCERN_PRIVATE_H */
