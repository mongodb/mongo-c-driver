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


#include <bson.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


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
   bson_uint32_t size;
   bson_uint32_t n_cpu;
   bson_uint32_t n_counters;
   bson_uint32_t infos_offset;
   bson_uint32_t values_offset;
   bson_uint8_t  padding[44];
} mongoc_counters_t;
#pragma pack(pop)


BSON_STATIC_ASSERT(sizeof(mongoc_counters_t) == 64);


typedef struct
{
   bson_int64_t slots[8];
} mongoc_counter_slots_t;


BSON_STATIC_ASSERT(sizeof(mongoc_counter_slots_t) == 64);


typedef struct
{
   mongoc_counter_slots_t *cpus;
} mongoc_counter_t;


static mongoc_counters_t *
mongoc_counters_new_from_pid (unsigned pid)
{
   mongoc_counters_t *counters;
   size_t size;
   void *mem;
   char name[32];
   int fd;

   snprintf(name, sizeof name, "/mongoc-%hu", pid);
   name[sizeof name-1] = '\0';

   if (-1 == (fd = shm_open(name, O_RDONLY, 0))) {
      perror("Failed to load shared memory segment");
      return NULL;
   }

   size = getpagesize();

   if (MAP_FAILED == (mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0))) {
      fprintf(stderr, "Failed to mmap shared memory segment of size: %u",
             (unsigned)size);
      close(fd);
      return NULL;
   }

   counters = (mongoc_counters_t *)mem;
   size = counters->size;
   munmap(mem, getpagesize());

   if (MAP_FAILED == (mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0))) {
      fprintf(stderr, "Failed to mmap shared memory segment of size: %u",
             (unsigned)size);
      close(fd);
      return NULL;
   }

   counters = (mongoc_counters_t *)mem;
   close(fd);

   return counters;
}


static void
mongoc_counters_destroy (mongoc_counters_t *counters)
{
   BSON_ASSERT(counters);
   munmap((void *)counters, counters->size);
}


static mongoc_counter_info_t *
mongoc_counters_get_infos (mongoc_counters_t *counters,
                           bson_uint32_t     *n_infos)
{
   mongoc_counter_info_t *info;
   char *base = (char *)counters;

   BSON_ASSERT(counters);
   BSON_ASSERT(n_infos);

   info = (mongoc_counter_info_t *)(base + counters->infos_offset);
   *n_infos = counters->n_counters;

   return info;
}


static bson_int64_t
mongoc_counters_get_value (mongoc_counters_t     *counters,
                           mongoc_counter_info_t *info,
                           mongoc_counter_t      *counter)
{
   bson_int64_t value = 0;
   unsigned i;

   for (i = 0; i < counters->n_cpu; i++) {
      value += counter->cpus[i].slots[info->slot];
   }

   return value;
}


static void
mongoc_counters_print_info (mongoc_counters_t     *counters,
                            mongoc_counter_info_t *info,
                            FILE                  *file)
{
   mongoc_counter_t ctr;
   bson_int64_t value;
   char *base;

   BSON_ASSERT(info);
   BSON_ASSERT(file);

   base = (char *)counters;
   ctr.cpus = (mongoc_counter_slots_t *)(base + info->offset);

   value = mongoc_counters_get_value(counters, info, &ctr);

   fprintf(file, "%24s : %-24s : %-50s : %lld\n",
           info->category, info->name, info->description,
           (long long)value);
}


int
main (int   argc,
      char *argv[])
{
   mongoc_counter_info_t *infos;
   mongoc_counters_t *counters;
   bson_uint32_t n_counters = 0;
   unsigned i;
   int pid;

   if (argc != 2) {
      fprintf(stderr, "usage: %s PID\n", argv[0]);
      return 1;
   }

   pid = strtol(argv[1], NULL, 10);
   if (!(counters = mongoc_counters_new_from_pid(pid))) {
      return 1;
   }

   infos = mongoc_counters_get_infos(counters, &n_counters);
   for (i = 0; i < n_counters; i++) {
      mongoc_counters_print_info(counters, &infos[i], stdout);
   }

   mongoc_counters_destroy(counters);

   return 0;
}
