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


#ifndef MONGOC_WRITE_CONCERN_H
#define MONGOC_WRITE_CONCERN_H


#include <bson.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_write_concern_t mongoc_write_concern_t;


mongoc_write_concern_t *mongoc_write_concern_new           (void);
void                    mongoc_write_concern_destroy       (mongoc_write_concern_t *write_concern);
void                    mongoc_write_concern_set_fsync     (mongoc_write_concern_t *write_concern,
                                                            bson_bool_t             fsync_);
void                    mongoc_write_concern_set_journal   (mongoc_write_concern_t *write_concern,
                                                            bson_bool_t             journal);
void                    mongoc_write_concern_set_w         (mongoc_write_concern_t *write_concern,
                                                            bson_int32_t            w);
void                    mongoc_write_concern_set_wtimeout  (mongoc_write_concern_t *write_concern,
                                                            bson_int32_t            wtimeout_msec);
void                    mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                                            bson_int32_t            wtimeout_msec);


BSON_END_DECLS


#endif /* MONGOC_WRITE_CONCERN_H */
