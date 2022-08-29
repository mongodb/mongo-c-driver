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
   _mongoc_http_request_init (&req->req);
   // The HTTP host of the IMDS server
   req->req.host = "169.254.169.254";
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
   bson_free ((void *) req->req.path);
   *req = (mcd_azure_imds_request){0};
}

bool
mcd_azure_access_token_try_init_from_json_str (mcd_azure_access_token *out,
                                               const char *json,
                                               int len,
                                               bson_error_t *error)
{
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

   if (!(access_token && resource && token_type)) {
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
      okay = true;
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
}

bool
mcd_azure_send_request_with_retries (const mongoc_http_request_t *req,
                                     mongoc_http_response_t *resp,
                                     enum mcd_azure_http_flags flags,
                                     bson_error_t *error)
{
   int t_wait_sec = 0;
   int http_5xx_limit = 10;
   while (1) {
      // Zero the response object:
      _mongoc_http_response_init (resp);
      // Do the actual request:
      const bool req_okay = _mongoc_http_send (req,
                                               10000,
                                               false, // No TLS
                                               NULL,  // No TLS options
                                               resp,
                                               error);
      if (!req_okay) {
         // There was an error sending the request (not an error from the
         // server)
         return false;
      }

      if (resp->status >= 500) {
         // An error on the server-side.
         if (http_5xx_limit == 0) {
            // There have been many 5xx errors in a row. Count this as a
            // failure. Azure wants us to retry on HTTP 500, but lets not get
            // stuck in a loop on that.
            break;
         }
         // We'll try again in one second
         _mongoc_usleep (1000 * 1000);
         // Subtract from the 5xx limit
         http_5xx_limit--;
         continue;
      }

      const bool too_many_reqs = resp->status == 429;
      const bool retry_404 =
         resp->status == 404 && (flags & MCD_AZURE_RETRY_ON_404);

      if (too_many_reqs || retry_404) {
         // Either the resource does not exist (yet), or the server detected too
         // many requests.
         if (t_wait_sec > 30) {
            // We've accumulated too much wait time. Break out.
            break;
         }
         // Wait a second.
         _mongoc_usleep (t_wait_sec * 1000 * 1000);
         // Double the wait time and add two seconds. This results in a growth
         // pattern of:
         // 0s -> 2s -> 6s -> 14s -> 30s -> <fail>
         t_wait_sec = (t_wait_sec * 2) + 2;
         _mongoc_http_response_cleanup (resp);
         continue;
      }

      // Other error, too many retries, or a success.
      break;
   }

   return true;
}
