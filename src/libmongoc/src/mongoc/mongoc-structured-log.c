/*
 * Copyright 2020 MongoDB, Inc.
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
mongoc_structured_log_default_handler (mongoc_structured_log_entry_t *entry,
                                       void *user_data);

static bson_once_t once = BSON_ONCE_INIT;
static bson_mutex_t gStructuredLogMutex;
static mongoc_structured_log_func_t gStructuredLogger =
   mongoc_structured_log_default_handler;
static void *gStructuredLoggerData;
static FILE *log_stream;

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

const bson_t *
mongoc_structured_log_entry_get_message (mongoc_structured_log_entry_t *entry)
{
   if (!entry->structured_message) {
      entry->structured_message =
         BCON_NEW ("message", BCON_UTF8 (entry->message));

      if (entry->build_message_func) {
         entry->build_message_func (entry);
      }
   }

   return entry->structured_message;
}

mongoc_structured_log_level_t
mongoc_structured_log_entry_get_level (
   const mongoc_structured_log_entry_t *entry)
{
   return entry->level;
}

mongoc_structured_log_component_t
mongoc_structured_log_entry_get_component (
   const mongoc_structured_log_entry_t *entry)
{
   return entry->component;
}

void
mongoc_structured_log_set_handler (mongoc_structured_log_func_t log_func,
                                   void *user_data)
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

static mongoc_structured_log_level_t
_mongoc_structured_log_get_log_level_from_env (const char *variable)
{
   const char *level = getenv (variable);

   if (!level) {
      return MONGOC_STRUCTURED_LOG_DEFAULT_LEVEL;
   } else if (!strcasecmp (level, "trace")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_TRACE;
   } else if (!strcasecmp (level, "debug")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_DEBUG;
   } else if (!strcasecmp (level, "info")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_INFO;
   } else if (!strcasecmp (level, "notice")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_NOTICE;
   } else if (!strcasecmp (level, "warn")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_WARNING;
   } else if (!strcasecmp (level, "error")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_ERROR;
   } else if (!strcasecmp (level, "critical")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_CRITICAL;
   } else if (!strcasecmp (level, "alert")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_ALERT;
   } else if (!strcasecmp (level, "emergency")) {
      return MONGOC_STRUCTURED_LOG_LEVEL_EMERGENCY;
   } else {
      MONGOC_ERROR (
         "Invalid log level %s read for variable %s", level, variable);
      exit (EXIT_FAILURE);
   }
}

static mongoc_structured_log_level_t
_mongoc_structured_log_get_log_level (
   mongoc_structured_log_component_t component)
{
   switch (component) {
   case MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND:
      return _mongoc_structured_log_get_log_level_from_env (
         "MONGODB_LOGGING_COMMAND");
   case MONGOC_STRUCTURED_LOG_COMPONENT_CONNECTION:
      return _mongoc_structured_log_get_log_level_from_env (
         "MONGODB_LOGGING_CONNECTION");
   case MONGOC_STRUCTURED_LOG_COMPONENT_SDAM:
      return _mongoc_structured_log_get_log_level_from_env (
         "MONGODB_LOGGING_SDAM");
   case MONGOC_STRUCTURED_LOG_COMPONENT_SERVER_SELECTION:
      return _mongoc_structured_log_get_log_level_from_env (
         "MONGODB_LOGGING_SERVER_SELECTION");
   default:
      MONGOC_ERROR ("Requesting log level for unsupported component %d",
                    component);
      exit (EXIT_FAILURE);
   }
}

static void
_mongoc_structured_log_initialize_stream ()
{
   const char *log_target = getenv ("MONGODB_LOGGING_PATH");
   bool log_to_stderr = !log_target || !strcmp (log_target, "stderr");

   log_stream = log_to_stderr ? stderr : fopen (log_target, "a");
   if (!log_stream) {
      MONGOC_ERROR ("Cannot open log file %s for writing", log_target);
      exit (EXIT_FAILURE);
   }
}

static FILE *
_mongoc_structured_log_get_stream ()
{
   if (!log_stream) {
      _mongoc_structured_log_initialize_stream ();
   }

   return log_stream;
}

static void
mongoc_structured_log_default_handler (mongoc_structured_log_entry_t *entry,
                                       void *user_data)
{
   mongoc_structured_log_level_t log_level =
      _mongoc_structured_log_get_log_level (
         mongoc_structured_log_entry_get_component (entry));

   if (log_level < mongoc_structured_log_entry_get_level (entry)) {
      return;
   }

   char *message =
      bson_as_json (mongoc_structured_log_entry_get_message (entry), NULL);

   fprintf (_mongoc_structured_log_get_stream (),
            "Structured log: %d, %d, %s\n",
            mongoc_structured_log_entry_get_level (entry),
            mongoc_structured_log_entry_get_component (entry),
            message);

   bson_free (message);
}

/* just for testing */
void
_mongoc_structured_log_get_handler (mongoc_structured_log_func_t *log_func,
                                    void **user_data)
{
   *log_func = gStructuredLogger;
   *user_data = gStructuredLoggerData;
}
