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


#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-cursor-cursorid-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-cursorid"


typedef struct
{
   bool        has_cursor;
   bool        in_first_batch;
   bson_iter_t first_batch_iter;
   bson_t      first_batch_inline;
} mongoc_cursor_cursorid_t;


static void *
_mongoc_cursor_cursorid_new (void)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = bson_malloc0 (sizeof *cid);

   RETURN (cid);
}


static void
_mongoc_cursor_cursorid_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_free (cursor->iface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


bool
_mongoc_cursor_cursorid_prime (mongoc_cursor_t *cursor)
{
   bool ret = true;
   mongoc_cursor_cursorid_t *cid;
   const bson_t *bson;
   bson_iter_t iter, child;
   const char *ns;

   ENTRY;

   cid = cursor->iface_data;

   if (!cid->has_cursor) {
      ret = _mongoc_cursor_next (cursor, &bson);

      cid->has_cursor = true;

      if (ret &&
          bson_iter_init_find (&iter, bson, "cursor") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter) &&
          bson_iter_recurse (&iter, &child)) {
         while (bson_iter_next (&child)) {
            if (BSON_ITER_IS_KEY (&child, "id")) {
               cursor->rpc.reply.cursor_id = bson_iter_as_int64 (&child);
            } else if (BSON_ITER_IS_KEY (&child, "ns")) {
               ns = bson_iter_utf8 (&child, &cursor->nslen);
               bson_strncpy (cursor->ns, ns, sizeof cursor->ns);
            } else if (BSON_ITER_IS_KEY (&child, "firstBatch")) {
               if (BSON_ITER_HOLDS_ARRAY (&child) &&
                   bson_iter_recurse (&child, &cid->first_batch_iter)) {
                  cid->in_first_batch = true;
               }
            }
         }

         cursor->is_command = false;
      } else {
         ret = false;
      }
   }

   return ret;
}


static bool
_mongoc_cursor_cursorid_next (mongoc_cursor_t *cursor,
                              const bson_t   **bson)
{
   bool ret;
   mongoc_cursor_cursorid_t *cid;
   const uint8_t *data = NULL;
   uint32_t data_len = 0;

   ENTRY;

   cid = cursor->iface_data;

   if (! cid->has_cursor) {
      if (! _mongoc_cursor_cursorid_prime (cursor)) {
         return false;
      }
   }

   if (cid->in_first_batch) {
      while (bson_iter_next (&cid->first_batch_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&cid->first_batch_iter)) {
            bson_iter_document (&cid->first_batch_iter, &data_len, &data);
            if (bson_init_static (&cid->first_batch_inline, data, data_len)) {
               *bson = &cid->first_batch_inline;
               RETURN (true);
            }
         }
      }
      cid->in_first_batch = false;
      cursor->end_of_event = true;
      if (!cursor->rpc.reply.cursor_id) {
         cursor->done = true;
         *bson = NULL;
         RETURN (false);
      }
   }

   ret = _mongoc_cursor_next (cursor, bson);

   RETURN (ret);
}


static mongoc_cursor_t *
_mongoc_cursor_cursorid_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone_;

   ENTRY;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_cursorid_init (clone_);

   RETURN (clone_);
}


static mongoc_cursor_interface_t gMongocCursorCursorid = {
   _mongoc_cursor_cursorid_clone,
   _mongoc_cursor_cursorid_destroy,
   NULL,
   _mongoc_cursor_cursorid_next,
};


void
_mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor)
{
   ENTRY;

   cursor->iface_data = _mongoc_cursor_cursorid_new ();

   memcpy (&cursor->iface, &gMongocCursorCursorid,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}


