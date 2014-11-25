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


#include "mongoc-index.h"
#include "mongoc-trace.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "gridfs_index"


static mongoc_index_opt_t gMongocIndexOptDefault = {
   1,
   0,
   0,
   NULL,
   0,
   0,
   -1,
   -1,
   NULL,
   NULL
};

static mongoc_index_opt_geo_t gMongocIndexOptGeoDefault = {
   26,
   -90,
   90,
   -1,
   2
};

static mongoc_index_opt_wt_t gMongocIndexOptWTDefault = {
   { MONGOC_INDEX_STORAGE_OPT_WIREDTIGER },
   ""
};

const mongoc_index_opt_t *
mongoc_index_opt_get_default (void)
{
   return &gMongocIndexOptDefault;
}

const mongoc_index_opt_geo_t *
mongoc_index_opt_geo_get_default (void)
{
   return &gMongocIndexOptGeoDefault;
}

const mongoc_index_opt_wt_t *
mongoc_index_opt_wt_get_default (void)
{
   return &gMongocIndexOptWTDefault;
}

void
mongoc_index_opt_init (mongoc_index_opt_t *opt)
{
   BSON_ASSERT (opt);

   memcpy (opt, &gMongocIndexOptDefault, sizeof *opt);
}

void
mongoc_index_opt_geo_init (mongoc_index_opt_geo_t *opt)
{
   BSON_ASSERT (opt);

   memcpy (opt, &gMongocIndexOptGeoDefault, sizeof *opt);
}

void mongoc_index_opt_wt_init (mongoc_index_opt_wt_t *opt)
{
   BSON_ASSERT(opt);

   memcpy (opt, &gMongocIndexOptWTDefault, sizeof *opt);
}
