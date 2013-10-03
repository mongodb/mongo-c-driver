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


#include <stdarg.h>
#include <time.h>

#include "mongoc-log.h"


static void
mongoc_log_default_handler (mongoc_log_level_t  log_level,
                            const char         *log_domain,
                            const char         *message,
                            void               *user_data);


static bson_mutex_t       gLogMutex = BSON_MUTEX_INITIALIZER;
static mongoc_log_func_t  gLogFunc = mongoc_log_default_handler;
static void              *gLogData;


void
mongoc_log_set_handler (mongoc_log_func_t  log_func,
                        void              *user_data)
{
   bson_mutex_lock(&gLogMutex);
   gLogFunc = log_func;
   bson_mutex_unlock(&gLogMutex);
}


void
mongoc_log (mongoc_log_level_t  log_level,
            const char         *log_domain,
            const char         *format,
            ...)
{
   va_list args;
   char *message;

   bson_return_if_fail(format);

   va_start(args, format);
   message = bson_strdupv_printf(format, args);
   va_end(args);

   bson_mutex_lock(&gLogMutex);
   gLogFunc(log_level, log_domain, message, gLogData);
   bson_mutex_unlock(&gLogMutex);

   bson_free(message);
}


static const char *
log_level_str (mongoc_log_level_t log_level)
{
   switch (log_level) {
   case MONGOC_LOG_LEVEL_ERROR:
      return "ERROR";
   case MONGOC_LOG_LEVEL_CRITICAL:
      return "CRITICAL";
   case MONGOC_LOG_LEVEL_WARNING:
      return "WARNING";
   case MONGOC_LOG_LEVEL_MESSAGE:
      return "MESSAGE";
   case MONGOC_LOG_LEVEL_INFO:
      return "INFO";
   case MONGOC_LOG_LEVEL_DEBUG:
      return "DEBUG";
   case MONGOC_LOG_LEVEL_TRACE:
      return "TRACE";
   default:
      return "UNKNOWN";
   }
}


static void
mongoc_log_default_handler (mongoc_log_level_t  log_level,
                            const char         *log_domain,
                            const char         *message,
                            void               *user_data)
{
   struct timeval tv;
   struct tm tt;
   time_t t;
   char nowstr[32];

   gettimeofday(&tv, NULL);
   t = tv.tv_sec;
   tt = *localtime(&t);

   strftime(nowstr, sizeof nowstr, "%Y/%m/%d %H:%M:%S", &tt);

   fprintf(stderr, "%s.%04ld: %8s: %s: %s\n",
           nowstr,
           tv.tv_usec / 1000L,
           log_level_str(log_level),
           log_domain,
           message);
}
