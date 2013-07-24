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


#pragma pack(push, 1)
typedef struct
{
   bson_uint32_t offset;
   bson_uint32_t slot;
   char          category[24];
   char          name[32];
   char          description[64];
} mongoc_counter_info_t;
#pragma pack(pop)


BSON_STATIC_ASSERT(sizeof(mongoc_counter_info_t) == 128);


#pragma pack(push, 1)
typedef struct
{
   bson_uint64_t size;
   bson_uint32_t n_cpu;
   bson_uint32_t n_counters;
   bson_uint32_t counters_offset;
   bson_uint8_t  padding[44];
} mongoc_counter_header_t;
#pragma pack(pop)


BSON_STATIC_ASSERT(sizeof(mongoc_counter_header_t) == 64);


#define COUNTER(N, ident, Category, Name, Description) \
   mongoc_counter_t __mongoc_counter_##N;
#include "mongoc-counters.defs"
#undef COUNTER


static void _mongoc_counters_init (void) __attribute__((constructor));


static void *
get_counter_shm (size_t size)
{
   void *mem;
   char name[32];
   int fd;

   size = MAX(size, getpagesize());

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

   close(fd);
   memset(mem, 0, size);

   return mem;
}


static void
mongoc_counter_info_init (mongoc_counter_info_t *info,
                          const char            *category,
                          const char            *name,
                          const char            *description)
{
   strncpy(info->category, category, sizeof info->category);
   strncpy(info->name, name, sizeof info->name);
   strncpy(info->description, description, sizeof info->description);

   info->category[sizeof info->category-1] = '\0';
   info->name[sizeof info->name-1] = '\0';
   info->description[sizeof info->description-1] = '\0';

   info->offset = 0;
   info->slot = 0;
}


static void
_mongoc_counters_init (void)
{
   mongoc_counter_header_t *hdr;
   mongoc_counter_slots_t *groups = NULL;
   mongoc_counter_info_t *info;
   bson_uint8_t *mem;
   size_t size;
   size_t info_size;
   int nslots = 0;
   int ngroups;
   int ncpu;
   int i = 0;

   ncpu = _mongoc_get_n_cpu();

#define COUNTER(_n, ident, _category, _name, _desc) \
   nslots = MAX(nslots, _n);
#include "mongoc-counters.defs"
#undef COUNTER

   nslots++;
   ngroups = (nslots / SLOTS_PER_CACHELINE) + 1;
   info_size = nslots * sizeof *info;
   size = ncpu * ngroups * sizeof(mongoc_counter_slots_t);

   mem = get_counter_shm(size + info_size);

   hdr = (void *)mem;
   hdr->size = size + info_size;
   hdr->n_cpu = _mongoc_get_n_cpu();
   hdr->n_counters = 0;
   hdr->counters_offset = sizeof *hdr;

   groups = (void *)(mem +
                     hdr->counters_offset +
                     (sizeof *info * nslots));

#define COUNTER(_n, ident, Category, Name, Desc) \
   __mongoc_counter_##_n.cpus = &groups[(_n / SLOTS_PER_CACHELINE) * ncpu]; \
   assert(__mongoc_counter_##_n.cpus); \
   info = (mongoc_counter_info_t *) \
      (mem + hdr->counters_offset + (sizeof *info * i++)); \
   mongoc_counter_info_init(info, Category, Name, Desc); \
   info->offset = ((bson_uint8_t *)__mongoc_counter_##_n.cpus) - mem; \
   info->slot = _n % 8; \
   hdr->n_counters++;
#include "mongoc-counters.defs"
#undef COUNTER
}
