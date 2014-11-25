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

#include "mongoc-sdam-scanner-private.h"
#include "mongoc-async-private.h"
#include "mongoc-async-cmd-private.h"
#include "utlist.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "sdam_scanner"

mongoc_sdam_scanner_t *
mongoc_sdam_scanner_new (mongoc_sdam_scanner_cb_t cb,
                         void                    *cb_data)
{
   mongoc_sdam_scanner_t *tm = bson_malloc0 (sizeof (*tm));

   tm->async = mongoc_async_new ();
   bson_init (&tm->ismaster_cmd);
   BSON_APPEND_INT32 (&tm->ismaster_cmd, "isMaster", 1);

   tm->cb = cb;
   tm->cb_data = cb_data;

   return tm;
}

void
mongoc_sdam_scanner_destroy (mongoc_sdam_scanner_t *tm)
{
   mongoc_async_destroy (tm->async);
   bson_destroy (&tm->ismaster_cmd);

   bson_free (tm);
}

uint32_t
mongoc_sdam_scanner_add (mongoc_sdam_scanner_t             *tm,
                         const mongoc_server_description_t *sd)
{
   mongoc_sdam_scanner_node_t *node = bson_malloc0 (sizeof (*node));

   /* TODO hook up streams */

   node->id = tm->seq++;

   return node->id;
}

static void
mongoc_sdam_scanner_node_destroy (mongoc_sdam_scanner_node_t *node)
{
   DL_DELETE (node->tm->nodes, node);

   mongoc_async_cmd_destroy (node->cmd);
   mongoc_stream_destroy (node->stream);
   bson_free (node);
}

void
mongoc_sdam_scanner_rm (mongoc_sdam_scanner_t *tm,
                        uint32_t               id)
{
   mongoc_sdam_scanner_node_t *ele, *tmp;

   DL_FOREACH_SAFE (tm->nodes, ele, tmp)
   {
      if (ele->id == id) {
         mongoc_sdam_scanner_node_destroy (ele);
         break;
      }

      if (ele->id > id) {
         break;
      }
   }
}

static void
mongoc_sdam_scanner_ismaster_handler (mongoc_async_cmd_result_t result,
                                      const bson_t             *bson,
                                      void                     *data,
                                      bson_error_t             *error)
{
   mongoc_sdam_scanner_node_t *node = (mongoc_sdam_scanner_node_t *)data;

   if (!node->tm->cb (node->id, bson, node->tm->cb_data, error)) {
      mongoc_sdam_scanner_node_destroy (node);
   } else if (!bson) {
      mongoc_stream_destroy (node->stream);
   }
}

void
mongoc_sdam_scanner_scan (mongoc_sdam_scanner_t *tm,
                          int32_t                timeout_msec)
{
   mongoc_sdam_scanner_node_t *node;

   DL_FOREACH (tm->nodes, node)
   {
      node->cmd = mongoc_async_cmd (tm->async, node->stream, "admin",
                                    &tm->ismaster_cmd,
                                    &mongoc_sdam_scanner_ismaster_handler, node,
                                    timeout_msec);
   }

   while (mongoc_async_run (tm->async, -1)) {
   }
}
