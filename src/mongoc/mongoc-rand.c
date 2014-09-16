/*
 * Copyright 2014 MongoDB, Inc.
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


#include "mongoc-config.h"

#include "mongoc-rand-private.h"

#ifndef MONGOC_OS_WIN32
#include <unistd.h>
#endif

static uint16_t
_mongoc_getpid (void)
{
   uint16_t pid;
#ifdef MONGOC_OS_WIN32
   DWORD real_pid;

   real_pid = GetCurrentProcessId ();
   pid = (real_pid & 0xFFFF) ^ ((real_pid >> 16) & 0xFFFF);
#else
   pid = getpid ();
#endif

   return pid;
}

uint32_t _mongoc_rand_new_seed() {
   struct timeval tv;
   unsigned int seed[3];

   bson_gettimeofday (&tv);
   seed[0] = (unsigned int)tv.tv_sec;
   seed[1] = (unsigned int)tv.tv_usec;
   seed[2] = _mongoc_getpid ();

   return seed[0] ^ seed[1] ^ seed[2];
}

uint32_t _mongoc_rand(uint32_t * seed) {
#ifdef BSON_OS_WIN32
   /* ms's runtime is multithreaded by default, so no rand_r */
   srand(*seed);
   *seed = rand();
   return seed;
#else
   return rand_r (seed);
#endif
}
