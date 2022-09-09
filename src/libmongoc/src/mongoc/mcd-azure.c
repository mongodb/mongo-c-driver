/**
 * Copyright 2022 MongoDB, Inc.
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

#include "./mcd-azure.h"

#include "mongoc-util-private.h"

#define AZURE_API_VERSION "2018-02-01"

static const char *const DEFAULT_METADATA_PATH =
   "/metadata/identity/oauth2/"
   "token?api-version=" AZURE_API_VERSION
   "&resource=https%3A%2F%2Fvault.azure.net";

void
mcd_azure_imds_request_init (mcd_azure_imds_request *req)
{
   BSON_ASSERT_PARAM (req);
   _mongoc_http_request_init (&req->req);
   // The HTTP host of the IMDS server
   req->req.host = "169.254.169.254";
   req->req.port = 80;
   // No body
   req->req.body = "";
   // We GET
   req->req.method = "GET";
   // 'Metadata: true' is required
   req->req.extra_headers = "Metadata: true\r\n"
                            "Accept: application/json\r\n";
   // The default path is suitable. In the future, we may want to add query
   // parameters to disambiguate a managed identity.
   req->req.path = bson_strdup (DEFAULT_METADATA_PATH);
}

void
mcd_azure_imds_request_destroy (mcd_azure_imds_request *req)
{
   BSON_ASSERT_PARAM (req);
   bson_free ((void *) req->req.path);
   *req = (mcd_azure_imds_request){0};
}

bool
mcd_azure_access_token_try_init_from_json_str (mcd_azure_access_token *out,
                                               const char *json,
                                               int len,
                                               bson_error_t *error)
{
   BSON_ASSERT_PARAM (out);
   BSON_ASSERT_PARAM (json);
   bool okay = false;

   if (len < 0) {
      // Detect from a null-terminated string
      len = strlen (json);
   }

   // Zero the output
   *out = (mcd_azure_access_token){0};

   // Parse the JSON data
   bson_t bson;
   if (!bson_init_from_json (&bson, json, len, error)) {
      return false;
   }

   bson_iter_t iter;
   // access_token
   bool found = bson_iter_init_find (&iter, &bson, "access_token");
   const char *const access_token =
      !found ? NULL : bson_iter_utf8 (&iter, NULL);
   // resource
   found = bson_iter_init_find (&iter, &bson, "resource");
   const char *const resource = !found ? NULL : bson_iter_utf8 (&iter, NULL);
   // token_type
   found = bson_iter_init_find (&iter, &bson, "token_type");
   const char *const token_type = !found ? NULL : bson_iter_utf8 (&iter, NULL);
   // expires_in
   found = bson_iter_init_find (&iter, &bson, "expires_in");
   uint32_t expires_in_len = 0;
   const char *const expires_in_str =
      !found ? NULL : bson_iter_utf8 (&iter, &expires_in_len);

   if (!(access_token && resource && token_type && expires_in_str)) {
      bson_set_error (
         error,
         MONGOC_ERROR_PROTOCOL_ERROR,
         64,
         "One or more required JSON properties are missing/invalid: data: %.*s",
         len,
         json);
   } else {
      // Set the output, duplicate each string
      *out = (mcd_azure_access_token){
         .access_token = bson_strdup (access_token),
         .resource = bson_strdup (resource),
         .token_type = bson_strdup (token_type),
      };
      // "expires_in" encodes the number of seconds since the issue time for
      // which the token will be valid. strtoll() will saturate on range errors
      // and return zero on parse errors.
      char *parse_end;
      long long s = strtoll (expires_in_str, &parse_end, 0);
      if (parse_end != expires_in_str + expires_in_len) {
         // Did not parse the entire string. Bad
         bson_set_error (
            error,
            MONGOC_ERROR_PROTOCOL,
            65,
            "Invalid 'expires_in' string \"%.*s\" from IMDS server",
            expires_in_len,
            expires_in_str);
      } else {
         out->expires_in = mcd_seconds (s);
         okay = true;
      }
   }

   bson_destroy (&bson);
   return okay;
}


void
mcd_azure_access_token_destroy (mcd_azure_access_token *c)
{
   bson_free (c->access_token);
   bson_free (c->resource);
   bson_free (c->token_type);
   c->access_token = NULL;
   c->resource = NULL;
   c->token_type = NULL;
}
