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


#include "mongoc-log.h"
#include "mongoc-write-concern.h"
#include "mongoc-write-concern-private.h"


static BSON_INLINE bson_bool_t
mongoc_write_concern_warn_frozen (mongoc_write_concern_t *write_concern)
{
   if (write_concern->frozen) {
      MONGOC_WARNING("Cannot modify a frozen write-concern.");
   }

   return write_concern->frozen;
}


/**
 * mongoc_write_concern_new:
 *
 * Create a new mongoc_write_concern_t.
 *
 * Returns: A newly allocated mongoc_write_concern_t. This should be freed
 *    with mongoc_write_concern_destroy().
 */
mongoc_write_concern_t *
mongoc_write_concern_new (void)
{
   mongoc_write_concern_t *write_concern;

   write_concern = bson_malloc0(sizeof *write_concern);
   return write_concern;
}


/**
 * mongoc_write_concern_destroy:
 * @write_concern: A mongoc_write_concern_t.
 *
 * Releases a mongoc_write_concern_t and all associated memory.
 */
void
mongoc_write_concern_destroy (mongoc_write_concern_t *write_concern)
{
   if (write_concern) {
      if (write_concern->compiled.len) {
         bson_destroy(&write_concern->compiled);
      }

      if (write_concern->tags.len) {
         bson_destroy(&write_concern->tags);
      }

      bson_free(write_concern);
   }
}


bson_bool_t
mongoc_write_concern_get_fsync (mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, FALSE);
   return write_concern->fsync_;
}


/**
 * mongoc_write_concern_set_fsync:
 * @write_concern: A mongoc_write_concern_t.
 * @fsync_: If the write concern requires fsync() by the server.
 *
 * Set if fsync() should be called on the server before acknowledging a
 * write request.
 */
void
mongoc_write_concern_set_fsync (mongoc_write_concern_t *write_concern,
                                bson_bool_t             fsync_)
{
   bson_return_if_fail(write_concern);

   if (!mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->fsync_ = fsync_;
   }
}


bson_bool_t
mongoc_write_concern_get_journal (mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, FALSE);
   return write_concern->journal;
}


/**
 * mongoc_write_concern_set_journal:
 * @write_concern: A mongoc_write_concern_t.
 * @journal: If the write should be journaled.
 *
 * Set if the write request should be journaled before acknowledging the
 * write request.
 */
void
mongoc_write_concern_set_journal (mongoc_write_concern_t *write_concern,
                                  bson_bool_t             journal)
{
   bson_return_if_fail(write_concern);

   if (!mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->journal = journal;
   }
}


bson_int32_t
mongoc_write_concern_get_w (mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, 0);
   return write_concern->w;
}


/**
 * mongoc_write_concern_set_w:
 * @w: The number of nodes for write or -1 for "majority".
 *
 * Sets the number of nodes that must acknowledge the write request before
 * acknowledging the write request to the client.
 *
 * You may specifiy @w as -1 to request that a "majority" of nodes
 * acknowledge the request.
 */
void
mongoc_write_concern_set_w (mongoc_write_concern_t *write_concern,
                            bson_int32_t            w)
{
   bson_return_if_fail(write_concern);

   if (!mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = w;
   }
}


bson_int32_t
mongoc_write_concern_get_wtimeout (mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, 0);
   return write_concern->wtimeout;
}


/**
 * mongoc_write_concern_set_wtimeout:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the number of milliseconds to wait before considering a write
 * request as failed.
 */
void
mongoc_write_concern_set_wtimeout (mongoc_write_concern_t *write_concern,
                                   bson_int32_t            wtimeout_msec)
{
   bson_return_if_fail(write_concern);

   if (!mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->wtimeout = wtimeout_msec;
   }
}


bson_bool_t
mongoc_write_concern_get_wmajority (mongoc_write_concern_t *write_concern)
{
   bson_return_val_if_fail(write_concern, FALSE);
   return (write_concern->w == -1);
}


/**
 * mongoc_write_concern_set_wmajority:
 * @write_concern: A mongoc_write_concern_t.
 * @wtimeout_msec: Number of milliseconds before timeout.
 *
 * Sets the "w" of a write concern to "majority". It is suggested that
 * you provide a reasonable @wtimeout_msec to wait before considering the
 * write request failed.
 */
void
mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                    bson_int32_t            wtimeout_msec)
{
   bson_return_if_fail(write_concern);

   if (!mongoc_write_concern_warn_frozen(write_concern)) {
      write_concern->w = -1;
      write_concern->wtimeout = wtimeout_msec;
   }
}


/**
 * mongoc_write_concern_freeze:
 * @write_concern: A mongoc_write_concern_t.
 *
 * This is an internal function.
 *
 * Freeze the write concern if necessary and compile the getlasterror command
 * associated with it.
 *
 * You may not modify the write concern further after calling this function.
 *
 * Returns: A bson_t that should not be modified or freed as it is owned by
 *    the mongoc_write_concern_t instance.
 */
const bson_t *
mongoc_write_concern_freeze (mongoc_write_concern_t *write_concern)
{
   bson_t *b;

   bson_return_val_if_fail(write_concern, NULL);

   b = &write_concern->compiled;

   if (!write_concern->frozen) {
      write_concern->frozen = TRUE;

      bson_init(b);
      bson_append_int32(b, "getlasterror", 12, 1);

      if (write_concern->tags.len) {
         bson_append_document(b, "w", 1, &write_concern->tags);
      } else if (write_concern->w == -1) {
         bson_append_utf8(b, "w", 1, "majority", 8);
      } else if (write_concern->w) {
         bson_append_int32(b, "w", 1, write_concern->w);
      }

      if (write_concern->fsync_) {
         bson_append_bool(b, "fsync", 5, TRUE);
      }

      if (write_concern->journal) {
         bson_append_bool(b, "j", 1, TRUE);
      }

      if (write_concern->wtimeout) {
         bson_append_int32(b, "wtimeout", 8, write_concern->wtimeout);
      }
   }

   return b;
}
