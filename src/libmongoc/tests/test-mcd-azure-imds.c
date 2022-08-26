#include <mongoc/mcd-azure.h>

#include "TestSuite.h"

#define STRING(...) #__VA_ARGS__

void
_test_oauth_parse (void)
{
   // Test that we can correct parse a JSON document from the IMSD sever
   bson_error_t error;
   mcd_azure_access_token token;
   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, "invalid json", -1, &error));
   ASSERT_CMPINT (error.domain, ==, BSON_ERROR_JSON);

   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, "{}", -1, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_PROTOCOL_ERROR, 64, "");

   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, STRING ({"access_token" : null}), -1, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_PROTOCOL_ERROR, 64, "");

   error = (bson_error_t){0};
   ASSERT (mcd_azure_access_token_try_init_from_json_str (
      &token,
      STRING ({
         "access_token" : "meow",
         "resource" : "something",
         "token_type" : "Bearer"
      }),
      -1,
      &error));
   ASSERT_ERROR_CONTAINS (error, 0, 0, "");
   ASSERT_CMPSTR (token.access_token, "meow");

   mcd_azure_access_token_destroy (&token);
}

void
_test_http_req (void)
{
   // Test generating an HTTP request for the IMDS server
   mcd_azure_imds_request req;
   mcd_azure_imds_request_init (&req);
   bson_string_t *req_str = _mongoc_http_render_request_head (&req.req);
   mcd_azure_imds_request_destroy (&req);
   // Assert that we composed exactly the request that we expected
   ASSERT_CMPSTR (req_str->str,
                  "GET "
                  "/metadata/identity/oauth2/"
                  "token?api-version=2018-02-01&resource=https%3A%2F%2Fvault."
                  "azure.net HTTP/1.0\r\n"
                  "Host: 169.254.169.254\r\n"
                  "Connection: close\r\n"
                  "Metadata: true\r\n"
                  "Accept: application/json\r\n"
                  "\r\n");
   bson_string_free (req_str, true);
}

void
test_mcd_azure_mid_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/azure/imds/http/parse", _test_oauth_parse);
   TestSuite_Add (suite, "/azure/imds/http/request", _test_http_req);
}
