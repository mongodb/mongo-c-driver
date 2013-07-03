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


#include "mongoc-write-concern.h"
#include "mongoc-write-concern-private.h"


mongoc_write_concern_t *
mongoc_write_concern_new (void)
{
   mongoc_write_concern_t *write_concern;

   write_concern = bson_malloc0(sizeof *write_concern);
   return write_concern;
}


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


void
mongoc_write_concern_set_fsync (mongoc_write_concern_t *write_concern,
                                bson_bool_t             fsync_)
{
   bson_return_if_fail(write_concern);
   write_concern->fsync_ = fsync_;
}


void
mongoc_write_concern_set_journal (mongoc_write_concern_t *write_concern,
                                  bson_bool_t             journal)
{
   bson_return_if_fail(write_concern);
   write_concern->journal = journal;
}


void
mongoc_write_concern_set_w (mongoc_write_concern_t *write_concern,
                            bson_int32_t            w)
{
   bson_return_if_fail(write_concern);
   write_concern->w = w;
}


void
mongoc_write_concern_set_wtimeout (mongoc_write_concern_t *write_concern,
                                   bson_int32_t            wtimeout_msec)
{
   bson_return_if_fail(write_concern);
   write_concern->wtimeout = wtimeout_msec;
}


void
mongoc_write_concern_set_wmajority (mongoc_write_concern_t *write_concern,
                                    bson_int32_t            wtimeout_msec)
{
   bson_return_if_fail(write_concern);
   write_concern->w = -1;
   write_concern->wtimeout = wtimeout_msec;
}
