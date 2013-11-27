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


#define _GNU_SOURCE

#include "mongoc-cursor.h"
#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-counters-private.h"
#include "mongoc-error.h"
#include "mongoc-log.h"
#include "mongoc-opcode.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "cursor-cursorid"

typedef struct mongoc_cursor_cursorid
{
   bson_bool_t has_cursor;
} mongoc_cursor_cursorid_t;


static void *
_mongoc_cursor_cursorid_new (void)
{
   mongoc_cursor_cursorid_t *cid;

   ENTRY;

   cid = bson_malloc0 (sizeof *cid);

   RETURN (cid);
}


void
_mongoc_cursor_cursorid_destroy (mongoc_cursor_t *cursor)
{
   ENTRY;

   bson_free (cursor->interface_data);
   _mongoc_cursor_destroy (cursor);

   EXIT;
}


bson_bool_t
_mongoc_cursor_cursorid_next (mongoc_cursor_t *cursor,
                              const bson_t   **bson)
{
   bson_bool_t ret;
   mongoc_cursor_cursorid_t *cid;
   bson_iter_t iter;
   bson_iter_t child;
   const char *ns;

   ENTRY;

   cid = cursor->interface_data;

   ret = _mongoc_cursor_next (cursor, bson);

   if (!cid->has_cursor) {
      cid->has_cursor = TRUE;

      if (ret &&
          bson_iter_init_find (&iter, *bson, "cursor") &&
          BSON_ITER_HOLDS_DOCUMENT (&iter) &&
          bson_iter_recurse (&iter, &child)) {
         while (bson_iter_next (&child)) {
            if (strcmp (bson_iter_key (&child), "id") == 0) {
               cursor->rpc.reply.cursor_id = bson_iter_int64 (&child);
            } else if (strcmp (bson_iter_key (&child), "ns") == 0) {
               ns = bson_iter_utf8 (&child, &cursor->nslen);
               strncpy (cursor->ns, ns, sizeof cursor->ns - 1);
            }
         }

         cursor->is_command = FALSE;

         ret = _mongoc_cursor_next (cursor, bson);
      }
   }


   RETURN (ret);
}


mongoc_cursor_t *
_mongoc_cursor_cursorid_clone (const mongoc_cursor_t *cursor)
{
   mongoc_cursor_t *clone;

   ENTRY;

   clone = _mongoc_cursor_clone (cursor);
   _mongoc_cursor_cursorid_init (clone);

   RETURN (clone);
}


static mongoc_cursor_interface_t _mongoc_cursor_cursorid = {
   &_mongoc_cursor_cursorid_clone,
   &_mongoc_cursor_cursorid_destroy,
   NULL,
   &_mongoc_cursor_cursorid_next,
   NULL,
   NULL,
};


void
_mongoc_cursor_cursorid_init (mongoc_cursor_t *cursor)
{
   ENTRY;

   cursor->interface_data = _mongoc_cursor_cursorid_new ();

   memcpy (&cursor->interface, &_mongoc_cursor_cursorid,
           sizeof (mongoc_cursor_interface_t));

   EXIT;
}


