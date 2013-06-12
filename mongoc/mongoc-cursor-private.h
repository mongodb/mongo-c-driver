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


#ifndef MONGOC_CURSOR_PRIVATE_H
#define MONGOC_CURSOR_PRIVATE_H


#include <bson.h>

#include "mongoc-client.h"
#include "mongoc-event-private.h"


BSON_BEGIN_DECLS


struct _mongoc_cursor_t
{
   mongoc_client_t *client;
   bson_uint32_t    hint;
   bson_uint32_t    stamp;

   bson_bool_t      done         : 1;
   bson_bool_t      failed       : 1;
   bson_bool_t      end_of_event : 1;

   bson_uint32_t    batch_size;

   char            *ns;
   bson_uint32_t    nslen;

   bson_error_t     error;

   mongoc_event_t   ev;
};


mongoc_cursor_t *mongoc_cursor_new (mongoc_client_t *client,
                                    bson_uint32_t    hint,
                                    bson_int32_t     request_id,
                                    const char      *ns,
                                    bson_uint32_t    nslen,
                                    bson_error_t    *error);


BSON_END_DECLS


#endif /* MONGOC_CURSOR_PRIVATE_H */
