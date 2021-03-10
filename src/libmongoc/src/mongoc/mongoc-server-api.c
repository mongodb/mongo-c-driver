/*
 * Copyright 2021 MongoDB, Inc.
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


#include "mongoc-config.h"
#include "mongoc-server-api-private.h"

const char *
mongoc_server_api_version_to_string (mongoc_server_api_version_t version)
{
   switch (version) {
   case MONGOC_SERVER_API_V1:
      return "1";
   default:
      return NULL;
   }
}

bool
mongoc_server_api_version_from_string (const char *version,
                                       mongoc_server_api_version_t *out)
{
   if (strcmp (version, "1") == 0) {
      *out = MONGOC_SERVER_API_V1;
      return true;
   }

   return false;
}

mongoc_server_api_t *
mongoc_server_api_new (mongoc_server_api_version_t version)
{
   mongoc_server_api_t *api;

   api = (mongoc_server_api_t *) bson_malloc0 (sizeof (mongoc_server_api_t));
   api->version = version;

   return api;
}

mongoc_server_api_t *
mongoc_server_api_copy (const mongoc_server_api_t *api)
{
   mongoc_server_api_t *copy;

   if (!api) {
      return NULL;
   }

   copy = (mongoc_server_api_t *) bson_malloc0 (sizeof (mongoc_server_api_t));
   copy->version = api->version;
   copy->strict_set = api->strict_set;
   copy->strict = api->strict;
   copy->deprecation_errors_set = api->deprecation_errors_set;
   copy->deprecation_errors = api->deprecation_errors;

   return copy;
}

void
mongoc_server_api_destroy (mongoc_server_api_t *api)
{
   if (!api) {
      return;
   }

   bson_free (api);
}

void
mongoc_server_api_strict (mongoc_server_api_t *api, bool strict)
{
   BSON_ASSERT (api);
   api->strict = strict;
   api->strict_set = true;
}

void
mongoc_server_api_deprecation_errors (mongoc_server_api_t *api,
                                      bool deprecation_errors)
{
   BSON_ASSERT (api);
   api->deprecation_errors = deprecation_errors;
   api->deprecation_errors_set = true;
}
