/*
 * Copyright 2013 MongoDB Inc.
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


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_index"

#include "mongoc-index.h"
#include "mongoc-trace.h"

static mongoc_index_opt_t default_opts = {
   1,
   0,
   0,
   NULL,
   0,
   0,
   -1,
   -1,
   NULL,
   NULL,
   NULL
};

const mongoc_index_opt_t * MONGOC_DEFAULT_INDEX_OPT = &default_opts;

void
mongoc_index_opt_init(mongoc_index_opt_t *opt)
{
   if (!opt) return;

   memcpy(opt, &default_opts, sizeof(*opt));
}
