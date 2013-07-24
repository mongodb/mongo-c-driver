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
   bson_uint64_t size;
   bson_uint32_t n_cpu;
   bson_uint32_t n_counters;
   bson_uint32_t counters_offset;
   bson_uint8_t  padding[44];
} mongoc_counter_header_t;
#pragma pack(pop)


typedef struct
{
   bson_int64_t slots[8];
} mongoc_slots_t;


long long
get_value (bson_uint8_t *mem,
           off_t         offset,
           int           ncpu,
           int           slot)
{
   mongoc_slots_t *slots;
   bson_int64_t value = 0;
   int i;

   slots = (mongoc_slots_t *)(mem + offset);
   for (i = 0; i < ncpu; i++) {
      value += slots[i].slots[slot];
   }

   return value;
}


int
main (int   argc,
      char *argv[])
{
   mongoc_counter_header_t *hdr;
   mongoc_counter_info_t *info;
   bson_uint32_t i;
   bson_uint8_t *mem;
   size_t size;
   pid_t pid;
   char name[32];
   int fd;

   if (argc != 2) {
      fprintf(stderr, "usage: %s PID\n", argv[0]);
      return 1;
   }

   pid = strtol(argv[1], NULL, 10);
   snprintf(name, sizeof name, "mongoc-%hu", (int)pid);
   name[sizeof name-1] = '\0';

   fd = shm_open(name, O_RDONLY, 0);
   if (fd == -1) {
      fprintf(stderr, "Failed to load shared memory segment.\n");
      return 1;
   }

   mem = mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap() shared memory segment.\n");
      return 1;
   }

   hdr = (void *)mem;
   size = hdr->size;
   munmap(mem, getpagesize());

   mem = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      fprintf(stderr, "Failed to mmap() shared memory segment.\n");
      return 1;
   }

   /*
    * TODO: Check that n_counters matches.
    */

   hdr = (void *)mem;

   for (i = 0; i < hdr->n_counters; i++) {
      info = (void *)(mem + hdr->counters_offset + (i * sizeof *info));
      printf("%24s : %-24s : %-48s : %lld\n",
             info->category, info->name, info->description,
             get_value(mem, info->offset, hdr->n_cpu, info->slot));
   }

   return 0;
}
