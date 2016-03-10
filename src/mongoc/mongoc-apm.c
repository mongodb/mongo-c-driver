/*
 * Copyright 2016 MongoDB, Inc.
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

#include "mongoc-apm-private.h"

/*
 * An Application Performance Management (APM) implementation, complying with
 * MongoDB's Command Monitoring Spec:
 *
 * https://github.com/mongodb/specifications/tree/master/source/command-monitoring
 */

/*
 * Private initializer / cleanup functions.
 */

void
mongoc_apm_command_started_init (mongoc_apm_command_started_t *event,
                                 const bson_t                 *command,
                                 const char                   *database_name,
                                 const char                   *command_name,
                                 int64_t                       request_id,
                                 int64_t                       operation_id,
                                 const mongoc_host_list_t     *host,
                                 uint32_t                      server_id,
                                 void                         *context)
{
   bson_iter_t iter;
   uint32_t len;
   const uint8_t *data;

   /* Command Monitoring Spec:
    *
    * In cases where queries or commands are embedded in a $query parameter
    * when a read preference is provided, they MUST be unwrapped and the value
    * of the $query attribute becomes the filter or the command in the started
    * event. The read preference will subsequently be dropped as it is
    * considered metadata and metadata is not currently provided in the command
    * events.
    */
   if (bson_has_field (command, "$readPreference")) {
      if (bson_iter_init_find (&iter, command, "$query") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter)) {
         bson_iter_document (&iter, &len, &data);
         event->command = bson_new_from_data (data, len);
      } else {
         /* $query should exist, but user could provide us a misformatted doc */
         event->command = bson_new ();
      }

      event->command_owned = true;
   } else {
      /* discard "const", we promise not to modify "command" */
      event->command = (bson_t *) command;
      event->command_owned = false;
   }

   event->database_name = database_name;
   event->command_name = command_name;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_started_cleanup (mongoc_apm_command_started_t *event)
{
   if (event->command_owned) {
      bson_destroy (event->command);
   }
}


void
mongoc_apm_command_succeeded_init (mongoc_apm_command_succeeded_t *event,
                                   int64_t                         duration,
                                   const bson_t                   *reply,
                                   const char                     *command_name,
                                   int64_t                         request_id,
                                   int64_t                         operation_id,
                                   const mongoc_host_list_t       *host,
                                   uint32_t                        server_id,
                                   void                           *context)
{
   BSON_ASSERT (reply);

   event->duration = duration;
   event->reply = reply;
   event->command_name = command_name;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_succeeded_cleanup (mongoc_apm_command_succeeded_t *event)
{
   /* no-op */
}


void
mongoc_apm_command_failed_init (mongoc_apm_command_failed_t *event,
                                int64_t                      duration,
                                const char                  *command_name,
                                const bson_error_t          *error,
                                int64_t                      request_id,
                                int64_t                      operation_id,
                                const mongoc_host_list_t    *host,
                                uint32_t                     server_id,
                                void                        *context)
{
   event->duration = duration;
   event->command_name = command_name;
   event->error = error;
   event->request_id = request_id;
   event->operation_id = operation_id;
   event->host = host;
   event->server_id = server_id;
   event->context = context;
}


void
mongoc_apm_command_failed_cleanup (mongoc_apm_command_failed_t *event)
{
   /* no-op */
}


/*
 * event field accessors
 */

/* command-started event fields */

const bson_t *
mongoc_apm_command_started_get_command (
   const mongoc_apm_command_started_t *event)
{
   return event->command;
}


const char *
mongoc_apm_command_started_get_database_name (
   const mongoc_apm_command_started_t *event)
{
   return event->database_name;
}


const char *
mongoc_apm_command_started_get_command_name (
   const mongoc_apm_command_started_t *event)
{
   return event->command_name;
}


int64_t
mongoc_apm_command_started_get_request_id (
   const mongoc_apm_command_started_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_started_get_operation_id (
   const mongoc_apm_command_started_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_started_get_host (const mongoc_apm_command_started_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_started_get_server_id (const mongoc_apm_command_started_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_started_get_context (
   const mongoc_apm_command_started_t *event)
{
   return event->context;
}


/* command-succeeded event fields */

int64_t
mongoc_apm_command_succeeded_get_duration (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->duration;
}


const bson_t *
mongoc_apm_command_succeeded_get_reply (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->reply;
}


const char *
mongoc_apm_command_succeeded_get_command_name (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->command_name;
}


int64_t
mongoc_apm_command_succeeded_get_request_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_succeeded_get_operation_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_succeeded_get_host (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_succeeded_get_server_id (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_succeeded_get_context (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->context;
}


/* command-failed event fields */

int64_t
mongoc_apm_command_failed_get_duration (
   const mongoc_apm_command_failed_t *event)
{
   return event->duration;
}


const char *
mongoc_apm_command_failed_get_command_name (
   const mongoc_apm_command_failed_t *event)
{
   return event->command_name;
}


void
mongoc_apm_command_failed_get_error (
   const mongoc_apm_command_failed_t *event,
   bson_error_t                      *error)
{
   memcpy (error, event->error, sizeof *event->error);
}


int64_t
mongoc_apm_command_failed_get_request_id (
   const mongoc_apm_command_failed_t *event)
{
   return event->request_id;
}


int64_t
mongoc_apm_command_failed_get_operation_id (
   const mongoc_apm_command_failed_t *event)
{
   return event->operation_id;
}


const mongoc_host_list_t *
mongoc_apm_command_failed_get_host (const mongoc_apm_command_failed_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_failed_get_server_id (const mongoc_apm_command_failed_t *event)
{
   return event->server_id;
}


void *
mongoc_apm_command_failed_get_context (const mongoc_apm_command_failed_t *event)
{
   return event->context;
}


/*
 * registering callbacks
 */

mongoc_apm_callbacks_t *
mongoc_apm_callbacks_new (void)
{
   size_t s = sizeof (mongoc_apm_callbacks_t);

   return (mongoc_apm_callbacks_t *) bson_malloc0 (s);
}


void
mongoc_apm_callbacks_destroy (mongoc_apm_callbacks_t *callbacks)
{
   bson_free (callbacks);
}


void
mongoc_apm_set_command_started_cb (
   mongoc_apm_callbacks_t          *callbacks,
   mongoc_apm_command_started_cb_t  cb)
{
   callbacks->started = cb;
}


void
mongoc_apm_set_command_succeeded_cb (
   mongoc_apm_callbacks_t            *callbacks,
   mongoc_apm_command_succeeded_cb_t  cb)
{
   callbacks->succeeded = cb;
}


void
mongoc_apm_set_command_failed_cb (
   mongoc_apm_callbacks_t         *callbacks,
   mongoc_apm_command_failed_cb_t  cb)
{
   callbacks->failed = cb;
}
