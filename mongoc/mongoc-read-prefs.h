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


#ifndef MONGOC_READ_PREFS_H
#define MONGOC_READ_PREFS_H


#include <bson.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_read_prefs_t mongoc_read_prefs_t;
typedef enum   _mongoc_read_mode_t  mongoc_read_mode_t;


enum _mongoc_read_mode_t
{
   MONGOC_READ_PRIMARY,
   MONGOC_READ_PRIMARY_PREFERRED,
   MONGOC_READ_SECONDARY,
   MONGOC_READ_SECONDARY_PREFERRED,
   MONGOC_READ_NEAREST,
};


mongoc_read_prefs_t *mongoc_read_prefs_new      (void);
void                 mongoc_read_prefs_destroy  (mongoc_read_prefs_t *read_prefs);
void                 mongoc_read_prefs_set_mode (mongoc_read_prefs_t *read_prefs,
                                                 mongoc_read_mode_t   mode);
void                 mongoc_read_prefs_set_tags (mongoc_read_prefs_t *read_prefs,
                                                 const bson_t        *tags);


BSON_END_DECLS


#endif /* MONGOC_READ_PREFS_H */
