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


#ifdef __linux__
#define _GNU_SOURCE
#include <sched.h>
#include <sys/sysinfo.h>
#endif

#include <bson.h>


BSON_BEGIN_DECLS


#ifdef __linux__
/*
 * TODO: Use rdtscp when available.
 */
#define ADD(v, count) v += count
#define CURCPU sched_getcpu()
#define NCPU   get_nprocs()
#else
#define ADD(v, count) __sync_fetch_and_add(&v, count)
#define CURCPU 0
#define NCPU   1
#endif


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


static inline int
_mongoc_get_n_cpu (void)
{
   return NCPU;
}


#define COUNTER(N, ident, Category, Name, Description) \
static inline void \
mongoc_counter_##ident##_add (bson_int64_t val) \
{ \
   ADD(__mongoc_counter_##N.cpus[CURCPU].slots[N%SLOTS_PER_CACHELINE], val); \
} \
static inline void \
mongoc_counter_##ident##_inc (void) \
{ \
   ADD(__mongoc_counter_##N.cpus[CURCPU].slots[N%SLOTS_PER_CACHELINE], 1); \
} \
static inline void \
mongoc_counter_##ident##_dec (void) \
{ \
   ADD(__mongoc_counter_##N.cpus[CURCPU].slots[N%SLOTS_PER_CACHELINE], -1); \
}
#include "mongoc-counters.defs"
#undef COUNTER


#undef ADD
#undef NCPU
#undef CURCPU


BSON_END_DECLS


#endif /* MONGOC_COUNTERS_H */
