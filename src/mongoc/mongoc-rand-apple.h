/*
 * Copyright 2015 MongoDB, Inc.
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

#ifndef MONGOC_RAND_APPLE_H
#define MONGOC_RAND_APPLE_H

#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
#error "Only <mongoc.h> can be included directly."
#endif

#ifdef MONGOC_APPLE_NATIVE_TLS

#include <bson.h>

BSON_BEGIN_DECLS


void
mongoc_rand_apple_seed (const void *buf,
                        int         num);

void
mongoc_rand_apple_add (const void *buf,
                       int         num,
                       double      entropy);

int
mongoc_rand_apple_status (void);


BSON_END_DECLS

/* API setup for Apple */
#define mongoc_rand_seed_impl mongoc_rand_apple_seed
#define mongoc_rand_add_impl mongoc_rand_apple_add
#define mongoc_rand_status_impl mongoc_rand_apple_status

#endif /* MONGOC_APPLE_NATIVE_TLS */
#endif /* MONGOC_RAND_APPLE_H */
