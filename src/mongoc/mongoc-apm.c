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


const mongoc_host_list_t *
mongoc_apm_command_started_get_host (const mongoc_apm_command_started_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_started_get_hint (const mongoc_apm_command_started_t *event)
{
   return event->hint;
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


const mongoc_host_list_t *
mongoc_apm_command_succeeded_get_host (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_succeeded_get_hint (
   const mongoc_apm_command_succeeded_t *event)
{
   return event->hint;
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
   memcpy (error, &event->error, sizeof *event->error);
}


int64_t
mongoc_apm_command_failed_get_request_id (
   const mongoc_apm_command_failed_t *event)
{
   return event->request_id;
}


const mongoc_host_list_t *
mongoc_apm_command_failed_get_host (const mongoc_apm_command_failed_t *event)
{
   return event->host;
}


uint32_t
mongoc_apm_command_failed_get_hint (const mongoc_apm_command_failed_t *event)
{
   return event->hint;
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
