/*
 * Copyright 2013 MongoDB, Inc.
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

#include "bson-compat.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bson-atomic.h"
#include "bson-clock.h"
#include "bson-context.h"
#include "bson-context-private.h"
#include "bson-memory.h"
#include "common-thread-private.h"
#include "common-md5-private.h"

#ifdef BSON_HAVE_SYSCALL_TID
#include <sys/syscall.h>
#endif


#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif


/*
 * Globals.
 */
static bson_context_t gContextDefault;
static int64_t gRandCounter = INT64_MIN;

static BSON_INLINE uint16_t
_bson_getpid (void)
{
   uint16_t pid;
#ifdef BSON_OS_WIN32
   DWORD real_pid;

   real_pid = GetCurrentProcessId ();
   pid = (real_pid & 0xFFFF) ^ ((real_pid >> 16) & 0xFFFF);
#else
   pid = getpid ();
#endif

   return pid;
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_set_oid_seq32_threadsafe --
 *
 *       Thread-safe version of 32-bit sequence generator.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_set_oid_seq32_threadsafe (bson_context_t *context, /* IN */
                                        bson_oid_t *oid)         /* OUT */
{
   int32_t seq = 1 + bson_atomic_int32_fetch_add (
                        &context->seq32, 1, bson_memory_order_seq_cst);
   seq = BSON_UINT32_TO_BE (seq);
   memcpy (&oid->bytes[9], ((uint8_t *) &seq) + 1, 3);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_set_oid_seq64_threadsafe --
 *
 *       Thread-safe 64-bit sequence generator.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */

static void
_bson_context_set_oid_seq64_threadsafe (bson_context_t *context, /* IN */
                                        bson_oid_t *oid)         /* OUT */
{
   int64_t seq = 1 + bson_atomic_int64_fetch_add (
                        &context->seq64, 1, bson_memory_order_seq_cst);

   seq = BSON_UINT64_TO_BE (seq);
   memcpy (&oid->bytes[4], &seq, sizeof (seq));
}

/*
 * --------------------------------------------------------------------------
 *
 * _bson_context_get_hostname
 *
 *       Gets the hostname of the machine, logs a warning on failure. "out"
 *       must be an array of HOST_NAME_MAX bytes.
 *
 * --------------------------------------------------------------------------
 */
static void
_bson_context_get_hostname (char *out)
{
   if (gethostname (out, HOST_NAME_MAX) != 0) {
      if (errno == ENAMETOOLONG) {
         fprintf (stderr,
                  "hostname exceeds %d characters, truncating.",
                  HOST_NAME_MAX);
      } else {
         fprintf (stderr, "unable to get hostname: %d", errno);
      }
   }
   out[HOST_NAME_MAX - 1] = '\0';
}

/*
 * The seed consists of the following hashed together:
 * - current time (with microsecond resolution)
 * - current pid
 * - current hostname
 * - The init-call counter
 */
struct _init_rand_params {
   struct timeval time;
   uint16_t thread_id;
   char hostname[HOST_NAME_MAX];
   int64_t rand_call_counter;
};

/* Arbitrary siphash key base number */
static const uint64_t SIPHASH_KEY_INIT = UINT64_C (0x1729) << 42;

static void
_bson_context_init_random (bson_context_t *context)
{
   /* Generated 32bit seed */
   uint32_t seed = 0;
   /* The message digest of the random params */
   uint8_t digest[16] = {0};
   /* The randomness parameters */
   struct _init_rand_params rand_params;
   bson_md5_t md5;

   /* Init each part of the randomness source: */
   memset (&rand_params, 0, sizeof rand_params);
   bson_gettimeofday (&rand_params.time);
   rand_params.thread_id = _bson_getpid ();
   context->gethostname (rand_params.hostname);
   rand_params.rand_call_counter =
      bson_atomic_int64_fetch_add (&gRandCounter, 1, bson_memory_order_seq_cst);

   /* Hash the param struct */
   COMMON_PREFIX (_bson_md5_init (&md5));
   COMMON_PREFIX (_bson_md5_append (
      &md5, (const uint8_t *) &rand_params, sizeof rand_params));
   COMMON_PREFIX (_bson_md5_finish (&md5, digest));

   /** Initialize the rand and sequence counters with our random digest */
   memcpy (context->randomness, digest, sizeof context->randomness);
   memcpy (&context->seq32, digest + 3, sizeof context->seq32);
   memcpy (&context->seq64, digest + 7, sizeof context->seq64);
}

static void
_bson_context_init (bson_context_t *context, bson_context_flags_t flags)
{
   context->flags = (int) flags;
   context->gethostname = _bson_context_get_hostname;

   context->oid_set_seq32 = _bson_context_set_oid_seq32_threadsafe;
   context->oid_set_seq64 = _bson_context_set_oid_seq64_threadsafe;

   _bson_context_init_random (context);
}


/*
 *--------------------------------------------------------------------------
 *
 * _bson_context_set_oid_rand --
 *
 *       Sets the process specific five byte random sequence in an oid.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       @oid is modified.
 *
 *--------------------------------------------------------------------------
 */
void
_bson_context_set_oid_rand (bson_context_t *context, bson_oid_t *oid)
{
   BSON_ASSERT (context);
   BSON_ASSERT (oid);

   memcpy (&oid->bytes[4], &context->randomness, 5);
}

/*
 *--------------------------------------------------------------------------
 *
 * _get_rand --
 *
 *       Gets a random four byte integer. Callers that will use the "rand"
 *       function must call "srand" prior.
 *
 * Returns:
 *       A random int32_t.
 *
 *--------------------------------------------------------------------------
 */
static int32_t
_get_rand (unsigned int *pseed)
{
   int32_t result = 0;
#ifdef BSON_HAVE_ARC4RANDOM_BUF
   arc4random_buf (&result, sizeof (result));
#elif defined(BSON_HAVE_RAND_R)
   result = rand_r (pseed);
#else
   /* ms's runtime is multithreaded by default, so no rand_r */
   /* no rand_r on android either */
   result = rand ();
#endif
   return result;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_new --
 *
 *       Initializes a new context with the flags specified.
 *
 *       In most cases, you want to call this with @flags set to
 *       BSON_CONTEXT_NONE.
 *
 *       If you are running on Linux, %BSON_CONTEXT_USE_TASK_ID can result
 *       in a healthy speedup for multi-threaded scenarios.
 *
 *       If you absolutely must have a single context for your application
 *       and use more than one thread, then %BSON_CONTEXT_THREAD_SAFE should
 *       be bitwise-or'd with your flags. This requires synchronization
 *       between threads.
 *
 *       If you expect your pid to change without notice, such as from an
 *       unexpected call to fork(), then specify
 *       %BSON_CONTEXT_DISABLE_PID_CACHE.
 *
 * Returns:
 *       A newly allocated bson_context_t that should be freed with
 *       bson_context_destroy().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_context_t *
bson_context_new (bson_context_flags_t flags)
{
   bson_context_t *context;

   context = bson_malloc0 (sizeof *context);
   _bson_context_init (context, flags);

   return context;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_destroy --
 *
 *       Cleans up a bson_context_t and releases any associated resources.
 *       This should be called when you are done using @context.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

void
bson_context_destroy (bson_context_t *context) /* IN */
{
   bson_free (context);
}


static BSON_ONCE_FUN (_bson_context_init_default)
{
   _bson_context_init (
      &gContextDefault,
      (BSON_CONTEXT_THREAD_SAFE | BSON_CONTEXT_DISABLE_PID_CACHE));
   BSON_ONCE_RETURN;
}


/*
 *--------------------------------------------------------------------------
 *
 * bson_context_get_default --
 *
 *       Fetches the default, thread-safe implementation of #bson_context_t.
 *       If you need faster generation, it is recommended you create your
 *       own #bson_context_t with bson_context_new().
 *
 * Returns:
 *       A shared instance to the default #bson_context_t. This should not
 *       be modified or freed.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bson_context_t *
bson_context_get_default (void)
{
   static bson_once_t once = BSON_ONCE_INIT;

   bson_once (&once, _bson_context_init_default);

   return &gContextDefault;
}
