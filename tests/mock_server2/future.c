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

#include <stdio.h>

#include "mongoc-array-private.h"
#include "mongoc-thread-private.h"
#include "future.h"


#define FUTURE_TIMEOUT_MS 10 * 1000

/* define functions like future_get_bool (future_t *future) */
#define MAKE_FUTURE_GETTER(TYPE) \
   TYPE \
   future_get_ ## TYPE (future_t *future) \
   { \
      if (future_wait (future)) { \
         return future_value_get_ ## TYPE (&future->return_value); \
      } \
      fprintf (stderr, "%s timed out\n", __FUNCTION__); \
      abort (); \
   }

#include "make-future-getters.def"
#undef MAKE_FUTURE_GETTER


future_t *
future_new (future_value_type_t return_type, int argc)
{
   future_t *future;

   future = bson_malloc0 (sizeof *future);
   future->return_value.type = return_type;
   future->argc = argc;
   /* TODO */
   /* the future_value_t's are initialized with type future_value_no_type */
   future->argv = bson_malloc0 ((size_t) argc * sizeof(future_value_t));
   mongoc_cond_init (&future->cond);
   mongoc_mutex_init (&future->mutex);

   return future;
}


future_t *future_new_copy (future_t *future)
{
   future_t *copy;

   mongoc_mutex_lock (&future->mutex);
   copy = future_new (future->return_value.type, future->argc);
   copy->return_value = future->return_value;
   memcpy (copy->argv, future->argv, future->argc * sizeof(future_value_t));
   mongoc_mutex_unlock (&future->mutex);

   return copy;
}


void
future_start (future_t *future,
              void *(*start_routine)(void *))
{
   int r = mongoc_thread_create (&future->thread,
                                 start_routine,
                                 (void *) future);

   assert (!r);
}


void
future_resolve (future_t *future, future_value_t return_value)
{
   mongoc_mutex_lock (&future->mutex);
   assert (!future->resolved);
   assert (future->return_value.type == return_value.type);
   future->return_value = return_value;
   future->resolved = true;
   mongoc_cond_signal (&future->cond);
   mongoc_mutex_unlock (&future->mutex);
}


bool
future_wait (future_t *future)
{
   /* TODO: configurable timeout */
   int64_t deadline = bson_get_monotonic_time () + FUTURE_TIMEOUT_MS * 1000;
   bool resolved;

   mongoc_mutex_lock (&future->mutex);
   while (!future->resolved && bson_get_monotonic_time () <= deadline) {
      mongoc_cond_timedwait (&future->cond, &future->mutex, FUTURE_TIMEOUT_MS);
   }
   resolved = future->resolved;
   mongoc_mutex_unlock (&future->mutex);

   return resolved;
}


void
future_destroy (future_t *future)
{
   bson_free (future->argv);
   mongoc_cond_destroy (&future->cond);
   mongoc_mutex_destroy (&future->mutex);
   bson_free (future);
}
