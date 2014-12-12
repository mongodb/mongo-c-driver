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

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async"

void
_mongoc_async_add_cmd (mongoc_async_t     *async,
                       mongoc_async_cmd_t *acmd)
{
   mongoc_async_cmd_t *tmp;

   DL_FOREACH (async->cmds, tmp)
   {
      if (tmp->expire_at >= acmd->expire_at) {
         DL_PREPEND_ELEM (async->cmds, tmp, acmd);
         return;
      }
   }

   DL_APPEND (async->cmds, acmd);

   async->ncmds++;
}

mongoc_async_cmd_t *
mongoc_async_cmd (mongoc_async_t       *async,
                  mongoc_stream_t      *stream,
                  const char           *dbname,
                  const bson_t         *cmd,
                  mongoc_async_cmd_cb_t cb,
                  void                 *cb_data,
                  int32_t               timeout_msec)
{
   mongoc_async_cmd_t *acmd = mongoc_async_cmd_new (async, stream, dbname, cmd,
                                                    cb, cb_data, timeout_msec);

   _mongoc_async_add_cmd (async, acmd);

   return acmd;
}

mongoc_async_t *
mongoc_async_new ()
{
   mongoc_async_t *async = bson_malloc0 (sizeof (*async));

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

bool
mongoc_async_run (mongoc_async_t *async,
                  int32_t         timeout_msec)
{
   mongoc_async_cmd_t *acmd, *tmp;
   mongoc_stream_poll_t *poll = NULL;
   int i;
   ssize_t nactive = 0;
   int64_t now;
   int64_t expire_at = 0;

   for (;;) {
      now = bson_get_monotonic_time ();

      if (expire_at == 0) {
         if (timeout_msec >= 0) {
            expire_at = now + (timeout_msec * 1000);
         } else {
            expire_at = -1;
         }
      } else if (timeout_msec >= 0) {
         timeout_msec = (expire_at - now) / 1000;
      }

      if (now > expire_at) {
         break;
      }

      DL_FOREACH_SAFE (async->cmds, acmd, tmp)
      {
         if (now > acmd->expire_at) {
            acmd->cb (MONGOC_ASYNC_CMD_TIMEOUT, NULL, (now - acmd->start_time), acmd->data,
                      &acmd->error);
            mongoc_async_cmd_destroy (acmd);
         } else {
            break;
         }
      }

      if (!async->ncmds) {
         break;
      }

      bson_free (poll);
      poll = bson_malloc0 (sizeof (*poll) * async->ncmds);

      i = 0;
      DL_FOREACH (async->cmds, acmd)
      {
         poll[i].stream = acmd->stream;
         poll[i].events = acmd->events;
         i++;
      }

      if (timeout_msec >= 0) {
         timeout_msec = MIN (timeout_msec, (async->cmds->expire_at - now) / 1000);
      } else {
         timeout_msec = (async->cmds->expire_at - now) / 1000;
      }

      nactive = mongoc_stream_poll (poll, async->ncmds, timeout_msec);

      if (nactive) {
         i = 0;

         DL_FOREACH_SAFE (async->cmds, acmd, tmp)
         {
            if (poll[i].revents & poll[i].events) {
               mongoc_async_cmd_run (acmd);

               nactive--;

               if (!nactive) {
                  break;
               }
            }

            i++;
         }
      }
   }

   bson_free (poll);

   return async->ncmds;
}
