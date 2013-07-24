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

#define _GNU_SOURCE

#include <assert.h>
#include <malloc.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>

#include "mongoc-counters-private.h"


#define COUNTER(N, ident, Category, Name, Description) \
   mongoc_counter_t __mongoc_counter_##N;
#include "mongoc-counters.defs"
#undef COUNTER


static void
_mongoc_counters_init (void)
{
   mongoc_counter_slots_t *groups = NULL;
   size_t size;
   int nslots = 0;
   int ngroups;
   int ncpu;

#ifdef __linux__
   ncpu = get_nprocs();
#elif defined(__APPLE__)
#error "TODO: Mac OS X support."
#else
#error "You're platform is not yet supported."
#endif

#define COUNTER(_n, ident, _category, _name, _desc) \
   nslots = MAX(nslots, _n);
#include "mongoc-counters.defs"
#undef COUNTER

   nslots++;
   ngroups = (nslots / SLOTS_PER_CACHELINE) + 1;

   size = ncpu * ngroups * sizeof(mongoc_counter_slots_t);
   posix_memalign((void **)&groups, 64, size);
   assert(groups);
   memset(groups, 0, size);

#define COUNTER(_n, ident, _category, _name, _desc) \
   __mongoc_counter_##_n.cpus = &groups[(_n / SLOTS_PER_CACHELINE) * ncpu]; \
   assert(__mongoc_counter_##_n.cpus);
#include "mongoc-counters.defs"
#undef COUNTER
}


void
(*mongoc_counters_init) (void)
   __attribute__((section (".ctors"))) = _mongoc_counters_init;
