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
#include "mongoc-log.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-cursorid"


static void *
_mongoc_cursor_cursorid_new (void)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)bson_malloc0 (sizeof *cid);

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
   mongoc_cursor_cursorid_t *cid;
   const bson_t *bson;
   bson_iter_t iter, child;
   const char *ns;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (_mongoc_cursor_run_command (cursor) &&
       _mongoc_read_from_buffer (cursor, &bson) &&
       bson_iter_init_find (&iter, bson, "cursor") &&
       BSON_ITER_HOLDS_DOCUMENT (&iter) &&
       bson_iter_recurse (&iter, &child)) {
      cid->has_cursor = true;

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

      RETURN (true);
   } else {
      cursor->failed = 1;
      RETURN (false);
   }
}


bool
_mongoc_cursor_cursorid_next (mongoc_cursor_t *cursor,
                              const bson_t   **bson)
{
   mongoc_cursor_cursorid_t *cid;
   const uint8_t *data = NULL;
   uint32_t data_len = 0;

   ENTRY;

   cid = (mongoc_cursor_cursorid_t *)cursor->iface_data;
   BSON_ASSERT (cid);

   if (! cid->has_cursor) {
      if (!_mongoc_cursor_cursorid_prime (cursor)) {
         GOTO (done);
      }
   }

   if (cid->in_first_batch) {
      while (bson_iter_next (&cid->first_batch_iter)) {
         if (BSON_ITER_HOLDS_DOCUMENT (&cid->first_batch_iter)) {
            bson_iter_document (&cid->first_batch_iter, &data_len, &data);
            if (bson_init_static (&cid->first_batch_inline, data, data_len)) {
               *bson = &cid->first_batch_inline;
               GOTO (done);
            }
         }
      }
      cid->in_first_batch = false;
      if (!cursor->rpc.reply.cursor_id) {
         cursor->end_of_event = true;
         GOTO (done);
      }
   }

   _mongoc_cursor_next (cursor, bson);

done:
   RETURN (*bson ? true : false);
}


static mongoc_cursor_t *
_mongoc_cursor_cursorid_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone_;

   ENTRY;

   clone_ = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_cursorid_init (clone_, &cursor->query);

   RETURN (clone_);
}


static mongoc_cursor_interface_t gMongocCursorCursorid = {
   _mongoc_cursor_cursorid_clone,
   _mongoc_cursor_cursorid_destroy,
   NULL,
   _mongoc_cursor_cursorid_next,
};


void
_mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor,
                              const bson_t    *command)
{
   ENTRY;

   bson_destroy (&cursor->query);
   bson_copy_to (command, &cursor->query);

   cursor->iface_data = _mongoc_cursor_cursorid_new ();

   memcpy (&cursor->iface, &gMongocCursorCursorid,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}


