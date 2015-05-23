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

#ifndef FUTURE_H
#define FUTURE_H

#include <bson.h>

#include "future_value.h"
#include "mongoc-thread-private.h"


typedef struct
{
   bool             resolved;
   future_value_t   return_value;
   int              argc;
   future_value_t  *argv;
   mongoc_cond_t    cond;
   mongoc_mutex_t   mutex;
   mongoc_thread_t  thread;
} future_t;

future_t *future_new (int argc);

future_t *future_new_copy (future_t *future);

void future_start (future_t *future,
                          void *(*start_routine)(void *));

void future_resolve (future_t *future, future_value_t return_value);

bool future_wait (future_t *future);

/* declare functions like future_get_bool (future_t *future) */
#define MAKE_FUTURE_GETTER(TYPE) \
   TYPE future_get_ ## TYPE (future_t *future);

#include "make-future-getters.def"
#undef MAKE_FUTURE_GETTER

void future_destroy (future_t *future);

#endif //FUTURE_H
