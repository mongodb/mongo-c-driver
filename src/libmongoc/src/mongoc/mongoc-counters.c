/*
 * Copyright 2009-present MongoDB, Inc.
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


#include <bson/bson.h>

#include <mlib/cmp.h>

#include <fcntl.h>

#include <stdlib.h>
#include <string.h>

#ifdef BSON_OS_UNIX
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <windows.h>
#endif

#include <common-atomic-private.h>
#include <mongoc/mongoc-counters-private.h>

#include <mongoc/mongoc-log.h>


#pragma pack(1)
typedef struct {
   uint32_t offset;
   uint32_t slot;
   char category[24];
   char name[32];
   char description[64];
} mongoc_counter_info_t;
#pragma pack()


BSON_STATIC_ASSERT2 (counter_info_t, sizeof (mongoc_counter_info_t) == 128);


#pragma pack(1)
typedef struct {
   uint32_t size;
   uint32_t n_cpu;
   uint32_t n_counters;
   uint32_t infos_offset;
   uint32_t values_offset;
   uint8_t padding[44];
} mongoc_counters_t;
#pragma pack()


BSON_STATIC_ASSERT2 (counters_t, sizeof (mongoc_counters_t) == 64);

#ifdef MONGOC_ENABLE_SHM_COUNTERS
/* When counters are enabled at compile time but fail to initiate a shared
 * memory segment, then fall back to a malloc'd segment. This malloc'd segment
 * isn't useful to anyone. But by using this fallback, the counter increment
 * functions can behave the same. I.e. they do not need to have a runtime check
 * for whether or not initiating the shared memory segment succeeded. */
static void *gCounterFallback = NULL;

#define COUNTER(ident, Category, Name, Description) mongoc_counter_t __mongoc_counter_##ident;
#include <mongoc/mongoc-counters.defs>
#undef COUNTER

/**
 * mongoc_counters_use_shm:
 *
 * Checks to see if counters should be exported over a shared memory segment.
 *
 * Returns: true if SHM is to be used.
 */
static bool
mongoc_counters_use_shm (void)
{
   return !getenv ("MONGOC_DISABLE_SHM");
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

   n_cpu = _mongoc_get_cpu_count ();
   n_groups = (LAST_COUNTER / SLOTS_PER_CACHELINE) + 1;
   size = (sizeof (mongoc_counters_t) + (LAST_COUNTER * sizeof (mongoc_counter_info_t)) +
           (n_cpu * n_groups * sizeof (mongoc_counter_slots_t)));

#ifdef BSON_OS_UNIX
   const long pg_sz = sysconf (_SC_PAGESIZE);
   if (mlib_cmp (size, >, pg_sz)) {
      return size;
   } else {
      BSON_ASSERT (pg_sz > 0);
      return (size_t) pg_sz;
   }
#else
   return size;
#endif
}
#endif


/**
 * mongoc_counters_destroy:
 *
 * Removes the shared memory segment for the current processes counters.
 */
void
_mongoc_counters_cleanup (void)
{
#ifdef MONGOC_ENABLE_SHM_COUNTERS
   if (gCounterFallback) {
      bson_free (gCounterFallback);
      gCounterFallback = NULL;
#if defined(BSON_OS_UNIX) && defined(MONGOC_ENABLE_SHM_COUNTERS)
   } else {
      char name[32];
      int pid;

      pid = getpid ();
      // Truncation is OK.
      int req = bson_snprintf (name, sizeof name, "/mongoc-%d", pid);
      BSON_ASSERT (req > 0);
      shm_unlink (name);
#endif
   }
#endif
}


#ifdef MONGOC_ENABLE_SHM_COUNTERS
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
#if defined(BSON_OS_UNIX) && defined(MONGOC_ENABLE_SHM_COUNTERS)
   void *mem;
   char name[32];
   int pid;
   int fd;

   if (!mongoc_counters_use_shm ()) {
      goto skip_shm;
   }

   pid = getpid ();
   // Truncation is OK.
   int req = bson_snprintf (name, sizeof name, "/mongoc-%d", pid);
   BSON_ASSERT (req > 0);

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
   if (-1 == (fd = shm_open (name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | O_NOFOLLOW))) {
      goto fail_noclean;
   }

#if defined(__APPLE__)
   // macOS does not have posix_fallocate available.
   if (-1 == ftruncate (fd, size)) {
      goto fail_cleanup;
   }
#else
   // Prefer posix_fallocate on Linux. posix_fallocate ensures that disk space
   // is allocated.
   if (0 != posix_fallocate (fd, 0, size)) {
      goto fail_cleanup;
   }
#endif // __APPLE__

   mem = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (mem == MAP_FAILED) {
      goto fail_cleanup;
   }

   close (fd);
   memset (mem, 0, size);

   return mem;

fail_cleanup:
   shm_unlink (name);
   close (fd);

fail_noclean:
   MONGOC_WARNING ("Falling back to malloc for counters.");

skip_shm:
#endif

   gCounterFallback = (void *) bson_malloc0 (size);

   return gCounterFallback;
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
mongoc_counters_register (
   mongoc_counters_t *counters, uint32_t num, const char *category, const char *name, const char *description)
{
   mongoc_counter_info_t *infos;
   char *segment;
   int n_cpu;

   BSON_ASSERT (counters);
   BSON_ASSERT (category);
   BSON_ASSERT (name);
   BSON_ASSERT (description);

   /*
    * Implementation Note:
    *
    * The memory barrier is required so that all of the above has been
    * completed. Then increment the n_counters so that a reading application
    * only knows about the counter after we have initialized it.
    */

   n_cpu = _mongoc_get_cpu_count ();
   segment = (char *) counters;

   infos = (mongoc_counter_info_t *) (segment + counters->infos_offset);
   infos = &infos[counters->n_counters];
   infos->slot = num % SLOTS_PER_CACHELINE;
   infos->offset = (counters->values_offset + ((num / SLOTS_PER_CACHELINE) * n_cpu * sizeof (mongoc_counter_slots_t)));

   bson_strncpy (infos->category, category, sizeof infos->category);
   bson_strncpy (infos->name, name, sizeof infos->name);
   bson_strncpy (infos->description, description, sizeof infos->description);

   mcommon_atomic_thread_fence ();

   counters->n_counters++;

   return infos->offset;
}
#endif

/**
 * mongoc_counters_init:
 *
 * Initializes the mongoc counters system. This should be run on library
 * initialization using the GCC constructor attribute.
 */
void
_mongoc_counters_init (void)
{
#ifdef MONGOC_ENABLE_SHM_COUNTERS
   mongoc_counter_info_t *info;
   mongoc_counters_t *counters;
   size_t infos_size;
   size_t off;
   size_t size;
   char *segment;

   size = mongoc_counters_calc_size ();
   segment = (char *) mongoc_counters_alloc (size);
   infos_size = LAST_COUNTER * sizeof *info;

   counters = (mongoc_counters_t *) segment;
   counters->n_cpu = _mongoc_get_cpu_count ();
   counters->n_counters = 0;
   counters->infos_offset = sizeof *counters;
   counters->values_offset = (uint32_t) (counters->infos_offset + infos_size);

   BSON_ASSERT ((counters->values_offset % 64) == 0);

#define COUNTER(ident, Category, Name, Desc)                                         \
   off = mongoc_counters_register (counters, COUNTER_##ident, Category, Name, Desc); \
   __mongoc_counter_##ident.cpus = (mongoc_counter_slots_t *) (segment + off);
#include <mongoc/mongoc-counters.defs>
#undef COUNTER

   /*
    * NOTE:
    *
    * Only update the size of the shared memory area for the client after
    * we have initialized the rest of the counters. Don't forget our memory
    * barrier to prevent compiler reordering.
    */
   mcommon_atomic_thread_fence ();
   counters->size = (uint32_t) size;
#endif
}
