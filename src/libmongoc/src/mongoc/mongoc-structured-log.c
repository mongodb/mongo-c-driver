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

static bson_once_t once = BSON_ONCE_INIT;
static bson_mutex_t gStructuredLogMutex;
static mongoc_structured_log_func_t gStructuredLogger = NULL;
static void *gStructuredLoggerData;

static BSON_ONCE_FUN (_mongoc_ensure_mutex_once)
{
   bson_mutex_init (&gStructuredLogMutex);

   BSON_ONCE_RETURN;
}

static bson_t* mongoc_log_structured_create_context(const char *message, va_list *ap)
{
   bson_t  *context;
   bcon_append_ctx_t ctx;

   context = BCON_NEW ("message", *message);

   bcon_append_ctx_init (&ctx);
   bcon_append_ctx_va (context, &ctx, ap);

   return context;
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

void mongoc_log_structured (mongoc_structured_log_level_t level,
                            mongoc_structured_log_component_t component,
                            const char *message,
                            ...)
{
   va_list ap;
   bson_t *context;

   if (!gStructuredLogger) {
      return;
   }

   va_start (ap, message);
   context = mongoc_log_structured_create_context(message, &ap);
   va_end (ap);

   bson_mutex_lock (&gStructuredLogMutex);
   gStructuredLogger (level, component, message, context, gStructuredLoggerData);
   bson_mutex_unlock (&gStructuredLogMutex);

   bson_destroy(context);
}

void mongoc_structured_log_command_started(mongoc_cmd_t *cmd,
                                           uint32_t request_id,
                                           // Driver connection ID
                                           // Server connection Id
                                           bool explicit_session)
{
   mongoc_log_structured(
      MONGOC_STRUCTURED_LOG_LEVEL_INFO,
      MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND,
      "Command started",
      "command",
      BCON_DOCUMENT (cmd->command), // @todo Convert to canonical extJson here?
      "databaseName",
      cmd->db_name,
      "commandName",
      cmd->command_name,
      "requestId",
      BCON_INT32 (request_id),
      "operationId",
      cmd->operation_id,
      "driverConnectionId",
      BCON_INT32 (0), // @todo Provide driverConnectionId
      "serverConnectionId",
      BCON_INT32 (0), // @todo Provide serverConnectionId
      "explicitSession",
      explicit_session
   );
}
