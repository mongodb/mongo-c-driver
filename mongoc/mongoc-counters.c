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
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mongoc-counters-private.h"


#define COUNTER(N, ident, Category, Name, Description) \
   mongoc_counter_t __mongoc_counter_##N;
#include "mongoc-counters.defs"
#undef COUNTER


void _mongoc_counters_init (void) __attribute__((constructor));


static void *
get_counter_shm (size_t size)
{
   void *mem;
   char name[32];
   int fd;

   snprintf(name, sizeof name, "/mongoc-%hu", getpid());
   name[sizeof name - 1] = '\0';

   fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
   if (fd == -1) {
      return NULL;
   }

   if (ftruncate(fd, size) == -1) {
      close(fd);
      return NULL;
   }

   mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      close(fd);
      return NULL;
   }

   memset(mem, 0, size);

   return mem;
}


void
_mongoc_counters_init (void)
{
   mongoc_counter_slots_t *groups = NULL;
   size_t size;
   int nslots = 0;
   int ngroups;
   int ncpu;

   ncpu = _mongoc_get_n_cpu();

#define COUNTER(_n, ident, _category, _name, _desc) \
   nslots = MAX(nslots, _n);
#include "mongoc-counters.defs"
#undef COUNTER

   nslots++;
   ngroups = (nslots / SLOTS_PER_CACHELINE) + 1;

   size = ncpu * ngroups * sizeof(mongoc_counter_slots_t);
   groups = get_counter_shm(size);
   assert(groups);

#define COUNTER(_n, ident, _category, _name, _desc) \
   __mongoc_counter_##_n.cpus = &groups[(_n / SLOTS_PER_CACHELINE) * ncpu]; \
   assert(__mongoc_counter_##_n.cpus);
#include "mongoc-counters.defs"
#undef COUNTER
}
