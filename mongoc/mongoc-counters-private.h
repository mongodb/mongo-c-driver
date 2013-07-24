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


#ifndef MONGOC_COUNTERS_H
#define MONGOC_COUNTERS_H


#define _GNU_SOURCE
#include <sched.h>
#include <bson.h>


BSON_BEGIN_DECLS


#ifndef SLOTS_PER_CACHELINE
#define SLOTS_PER_CACHELINE 8
#endif


typedef struct
{
   bson_int64_t slots[SLOTS_PER_CACHELINE];
} mongoc_counter_slots_t;


typedef struct
{
   mongoc_counter_slots_t *cpus;
} mongoc_counter_t;


#define COUNTER(N, ident, Category, Name, Description) \
   extern mongoc_counter_t __mongoc_counter_##N;
#include "mongoc-counters.defs"
#undef COUNTER


#define COUNTER(N, ident, Category, Name, Description) \
static inline void \
mongoc_counter_##ident##_inc (void) \
{ \
   volatile int cpu = sched_getcpu(); \
   ++__mongoc_counter_##N.cpus[cpu].slots[N/SLOTS_PER_CACHELINE]; \
} \
static inline void \
mongoc_counter_##ident##_dec (void) \
{ \
   volatile int cpu = sched_getcpu(); \
   --__mongoc_counter_##N.cpus[cpu].slots[N/SLOTS_PER_CACHELINE]; \
}
#include "mongoc-counters.defs"
#undef COUNTER


BSON_END_DECLS


#endif /* MONGOC_COUNTERS_H */
