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


#pragma pack(1)
typedef struct
{
   bson_uint32_t offset;
   bson_uint32_t slot;
   char          category[24];
   char          name[32];
   char          description[64];
} mongoc_counter_info_t;
#pragma pack()


BSON_STATIC_ASSERT(sizeof(mongoc_counter_info_t) == 128);


#pragma pack(1)
typedef struct
{
   bson_uint32_t size;
   bson_uint32_t n_cpu;
   bson_uint32_t n_counters;
   bson_uint32_t infos_offset;
   bson_uint32_t values_offset;
   bson_uint8_t  padding[44];
} mongoc_counters_t;
#pragma pack()


BSON_STATIC_ASSERT(sizeof(mongoc_counters_t) == 64);


#define COUNTER(ident, Category, Name, Description) \
   mongoc_counter_t __mongoc_counter_##ident;
#include "mongoc-counters.defs"
#undef COUNTER


/**
 * mongoc_counters_use_shm:
 *
 * Checks to see if counters should be exported over a shared memory segment.
 *
 * Returns: TRUE if SHM is to be used.
 */
static bson_bool_t
mongoc_counters_use_shm (void)
{
   return !getenv("MONGOC_DISABLE_SHM");
}


/**
 * mongoc_counters_calc_size:
 *
 * Returns the number of bytes required for the shared memory segment of
 * the process. This segment contains the various statistical counters for
 * the process.
 *
 * Returns: The number of bytes required.
 */
static size_t
mongoc_counters_calc_size (void)
{
   size_t n_cpu;
   size_t n_groups;
   size_t size;

   n_cpu = _mongoc_get_n_cpu();
   n_groups = (LAST_COUNTER / SLOTS_PER_CACHELINE) + 1;
   size = (sizeof(mongoc_counters_t) +
           (LAST_COUNTER * sizeof(mongoc_counter_info_t)) +
           (n_cpu * n_groups * sizeof(mongoc_counter_slots_t)));

   return MAX(getpagesize(), size);
}


/**
 * mongoc_counters_destroy:
 *
 * Removes the shared memory segment for the current processes counters.
 */
static void
mongoc_counters_destroy (void)
{
   char name[32];
   int pid;

   pid = getpid();
   snprintf(name, sizeof name, "/mongoc-%hu", pid);
   name[sizeof name - 1] = '\0';
   shm_unlink(name);
}


/**
 * mongoc_counters_alloc:
 * @size: The size of the shared memory segment.
 *
 * This function allocates the shared memory segment for use by counters
 * within the process.
 *
 * Returns: A shared memory segment, or malloc'd memory on failure.
 */
static void *
mongoc_counters_alloc (size_t size)
{
   void *mem;
   char name[32];
   int pid;
   int fd;

   if (!mongoc_counters_use_shm()) {
      goto use_malloc;
   }

   pid = getpid();
   snprintf(name, sizeof name, "/mongoc-%hu", pid);
   name[sizeof name - 1] = '\0';

   if (-1 == (fd = shm_open(name, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR))) {
      goto use_malloc;
   }

   if (-1 == ftruncate(fd, size)) {
      goto failure;
   }

   mem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      goto failure;
   }

   close(fd);
   memset(mem, 0, size);
   atexit(mongoc_counters_destroy);

   return mem;

failure:
   shm_unlink(name);
   close(fd);

use_malloc:
   return bson_malloc0(size);
}


/**
 * mongoc_counters_register:
 * @counters: A mongoc_counter_t.
 * @num: The counter number.
 * @category: The counter category.
 * @name: THe counter name.
 * @description The counter description.
 *
 * Registers a new counter in the memory segment for counters. If the counters
 * are exported over shared memory, it will be made available.
 *
 * Returns: The offset to the data for the counters values.
 */
static size_t
mongoc_counters_register (mongoc_counters_t *counters,
                          bson_uint32_t      num,
                          const char        *category,
                          const char        *name,
                          const char        *description)
{
   mongoc_counter_info_t *infos;
   char *segment;
   int n_cpu;

   BSON_ASSERT(counters);
   BSON_ASSERT(category);
   BSON_ASSERT(name);
   BSON_ASSERT(description);

   n_cpu = _mongoc_get_n_cpu();
   segment = (char *)counters;

   infos = (mongoc_counter_info_t *)(segment + counters->infos_offset);
   infos = &infos[counters->n_counters++];
   infos->slot = num % SLOTS_PER_CACHELINE;
   infos->offset = (counters->values_offset +
                    ((num / SLOTS_PER_CACHELINE) *
                     n_cpu * sizeof(mongoc_counter_slots_t)));

   strncpy(infos->category, category, sizeof infos->category);
   strncpy(infos->name, name, sizeof infos->name);
   strncpy(infos->description, description, sizeof infos->description);
   infos->category[sizeof infos->category-1] = '\0';
   infos->name[sizeof infos->name-1] = '\0';
   infos->description[sizeof infos->description-1] = '\0';

   return infos->offset;
}


static void
mongoc_counters_init (void) __attribute__((constructor));


/**
 * mongoc_counters_init:
 *
 * Initializes the mongoc counters system. This should be run on library
 * initialization using the GCC constructor attribute.
 */
static void
mongoc_counters_init (void)
{
   mongoc_counter_info_t *info;
   mongoc_counters_t *counters;
   size_t infos_size;
   size_t off;
   size_t size;
   char *segment;

   size = mongoc_counters_calc_size();
   segment = mongoc_counters_alloc(size);
   infos_size = LAST_COUNTER * sizeof *info;

   counters = (mongoc_counters_t *)segment;
   counters->size = size;
   counters->n_cpu = _mongoc_get_n_cpu();
   counters->n_counters = 0;
   counters->infos_offset = sizeof *counters;
   counters->values_offset = counters->infos_offset + infos_size;

#define COUNTER(ident, Category, Name, Desc) \
   off = mongoc_counters_register(counters, COUNTER_##ident, Category, Name, Desc); \
   __mongoc_counter_##ident.cpus = (void *)(segment + off);
#include "mongoc-counters.defs"
#undef COUNTER

}
