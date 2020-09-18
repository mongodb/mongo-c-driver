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


#if defined(__linux__)
#include <sys/syscall.h>
#elif defined(_WIN32)
#include <process.h>
#elif defined(__FreeBSD__)
#include <sys/thr.h>
#else
#include <unistd.h>
#endif
#include <stdarg.h>
#include <time.h>

#include "mongoc-structured-log.h"
#include "mongoc-structured-log-private.h"
#include "mongoc-thread-private.h"

static void
mongoc_structured_log_default_handler (mongoc_structured_log_entry_t *entry, void *user_data);

static bson_once_t once = BSON_ONCE_INIT;
static bson_mutex_t gStructuredLogMutex;
static mongoc_structured_log_func_t gStructuredLogger = mongoc_structured_log_default_handler;
static void *gStructuredLoggerData;

static BSON_ONCE_FUN (_mongoc_ensure_mutex_once)
{
   bson_mutex_init (&gStructuredLogMutex);

   BSON_ONCE_RETURN;
}

static void
mongoc_structured_log_entry_destroy (mongoc_structured_log_entry_t *entry)
{
   if (entry->structured_message) {
      bson_free (entry->structured_message);
   }
}

const bson_t*
mongoc_structured_log_entry_get_message (mongoc_structured_log_entry_t *entry)
{
   if (!entry->structured_message) {
      entry->structured_message = BCON_NEW ("message", BCON_UTF8 (entry->message));

      if (entry->build_message_func) {
         entry->build_message_func (entry);
      }
   }

   return entry->structured_message;
}

mongoc_structured_log_level_t
mongoc_structured_log_entry_get_level (const mongoc_structured_log_entry_t *entry)
{
   return entry->level;
}

mongoc_structured_log_component_t
mongoc_structured_log_entry_get_component (const mongoc_structured_log_entry_t *entry)
{
   return entry->component;
}

void
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func, void *user_data)
{
   bson_once (&once, &_mongoc_ensure_mutex_once);

   bson_mutex_lock (&gStructuredLogMutex);
   gStructuredLogger = log_func;
   gStructuredLoggerData = user_data;
   bson_mutex_unlock (&gStructuredLogMutex);
}

void
mongoc_structured_log (mongoc_structured_log_level_t level,
                       mongoc_structured_log_component_t component,
                       const char *message,
                       mongoc_structured_log_build_message_t build_message_func,
                       void *structured_message_data)
{
   mongoc_structured_log_entry_t entry = {
      level,
      component,
      message,
      NULL,
      build_message_func,
      structured_message_data,
   };

   if (!gStructuredLogger) {
      return;
   }

   bson_mutex_lock (&gStructuredLogMutex);
   gStructuredLogger (&entry, gStructuredLoggerData);
   bson_mutex_unlock (&gStructuredLogMutex);

   mongoc_structured_log_entry_destroy (&entry);
}

static void
mongoc_structured_log_default_handler (mongoc_structured_log_entry_t *entry, void *user_data)
{
   char *message = bson_as_json (mongoc_structured_log_entry_get_message (entry), NULL);

   fprintf (stderr,
            "Structured log: %d, %d, %s\n",
            mongoc_structured_log_entry_get_level (entry),
            mongoc_structured_log_entry_get_component (entry),
            message);

   bson_free (message);
}
