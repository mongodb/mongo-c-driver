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


#include <bson.h>

#include "mongoc-async-private.h"
#include "mongoc-async-cmd-private.h"
#include "utlist.h"
#include "mongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async"

mongoc_async_cmd_t *
mongoc_async_cmd (mongoc_async_t *async,
                  mongoc_stream_t *stream,
                  mongoc_async_cmd_setup_t setup,
                  void *setup_ctx,
                  const char *dbname,
                  const bson_t *cmd,
                  mongoc_async_cmd_cb_t cb,
                  void *cb_data,
                  int64_t timeout_msec)
{
   return mongoc_async_cmd_new (
      async, stream, setup, setup_ctx, dbname, cmd, cb, cb_data, timeout_msec);
}

mongoc_async_t *
mongoc_async_new ()
{
   mongoc_async_t *async = (mongoc_async_t *) bson_malloc0 (sizeof (*async));

   return async;
}

void
mongoc_async_destroy (mongoc_async_t *async)
{
   mongoc_async_cmd_t *acmd, *tmp;

   DL_FOREACH_SAFE (async->cmds, acmd, tmp)
   {
      mongoc_async_cmd_destroy (acmd);
   }

   bson_free (async);
}

void
mongoc_async_run (mongoc_async_t *async, int64_t timeout_msec)
{
   mongoc_async_cmd_t *acmd, *tmp;
   mongoc_stream_poll_t *poller = NULL;
   int i;
   ssize_t nactive;
   int64_t now;
   int64_t expire_at;
   int64_t poll_timeout_msec;
   size_t poll_size;

   BSON_ASSERT (timeout_msec > 0);

   now = bson_get_monotonic_time ();
   expire_at = now + timeout_msec * 1000;
   poll_size = 0;

   while (async->ncmds) {
      /* ncmds grows if we discover a replica & start calling ismaster on it */
      if (poll_size < async->ncmds) {
         poller = (mongoc_stream_poll_t *) bson_realloc (
            poller, sizeof (*poller) * async->ncmds);

         poll_size = async->ncmds;
      }

      i = 0;
      DL_FOREACH (async->cmds, acmd)
      {
         poller[i].stream = acmd->stream;
         poller[i].events = acmd->events;
         poller[i].revents = 0;
         i++;
      }

      poll_timeout_msec = (expire_at - now) / 1000;
      BSON_ASSERT (poll_timeout_msec < INT32_MAX);
      nactive =
         mongoc_stream_poll (poller, async->ncmds, (int32_t) poll_timeout_msec);

      if (nactive) {
         i = 0;

         DL_FOREACH_SAFE (async->cmds, acmd, tmp)
         {
            if (poller[i].revents & (POLLERR | POLLHUP)) {
               int hup = poller[i].revents & POLLHUP;
               if (acmd->state == MONGOC_ASYNC_CMD_SEND) {
                  bson_set_error (&acmd->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_CONNECT,
                                  hup ? "connection refused"
                                      : "unknown connection error");
               } else {
                  bson_set_error (&acmd->error,
                                  MONGOC_ERROR_STREAM,
                                  MONGOC_ERROR_STREAM_SOCKET,
                                  hup ? "connection closed"
                                      : "unknown socket error");
               }

               acmd->state = MONGOC_ASYNC_CMD_ERROR_STATE;
            }

            if (acmd->state == MONGOC_ASYNC_CMD_ERROR_STATE ||
                (poller[i].revents & poller[i].events)) {
               mongoc_async_cmd_run (acmd);
               nactive--;

               if (!nactive) {
                  break;
               }
            }

            i++;
         }
      }

      now = bson_get_monotonic_time ();
      if (now > expire_at) {
         break;
      }
   }

   if (poll_size) {
      bson_free (poller);
   }

   /* commands that succeeded or failed already have been removed from the
    * list and freed. therefore, all remaining commands have timed out. */
   DL_FOREACH_SAFE (async->cmds, acmd, tmp)
   {
      bson_set_error (&acmd->error,
                      MONGOC_ERROR_STREAM,
                      MONGOC_ERROR_STREAM_CONNECT,
                      acmd->state == MONGOC_ASYNC_CMD_SEND
                         ? "connection timeout"
                         : "socket timeout");

      acmd->cb (MONGOC_ASYNC_CMD_TIMEOUT,
                NULL,
                (now - acmd->start_time) / 1000,
                acmd->data,
                &acmd->error);
      mongoc_async_cmd_destroy (acmd);
   }
}
