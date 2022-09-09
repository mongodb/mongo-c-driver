#include <mongoc/mcd-azure.h>

#include <mongoc/mongoc-host-list-private.h>

#include "TestSuite.h"

#define RAW_STRING(...) #__VA_ARGS__

static void
_test_oauth_parse (void)
{
   // Test that we can correctly parse a JSON document from the IMDS sever
   bson_error_t error;
   mcd_azure_access_token token;
   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, "invalid json", -1, &error));
   ASSERT_CMPUINT32 (error.domain, ==, BSON_ERROR_JSON);

   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, "{}", -1, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_PROTOCOL_ERROR, 64, "");

   ASSERT (!mcd_azure_access_token_try_init_from_json_str (
      &token, RAW_STRING ({"access_token" : null}), -1, &error));
   ASSERT_ERROR_CONTAINS (error, MONGOC_ERROR_PROTOCOL_ERROR, 64, "");

   error = (bson_error_t){0};
   ASSERT (mcd_azure_access_token_try_init_from_json_str (
      &token,
      RAW_STRING ({
         "access_token" : "meow",
         "resource" : "something",
         "expires_in" : "1234",
         "token_type" : "Bearer"
      }),
      -1,
      &error));
   ASSERT_ERROR_CONTAINS (error, 0, 0, "");
   ASSERT_CMPSTR (token.access_token, "meow");

   mcd_azure_access_token_destroy (&token);
}

static void
_test_http_req (void)
{
   // Test generating an HTTP request for the IMDS server
   mcd_azure_imds_request req;
   mcd_azure_imds_request_init (&req, "example.com", 9879);
   bson_string_t *req_str = _mongoc_http_render_request_head (&req.req);
   mcd_azure_imds_request_destroy (&req);
   // Assert that we composed exactly the request that we expected
   ASSERT_CMPSTR (req_str->str,
                  "GET "
                  "/metadata/identity/oauth2/"
                  "token?api-version=2018-02-01&resource=https%3A%2F%2Fvault."
                  "azure.net HTTP/1.0\r\n"
                  "Host: example.com:9879\r\n"
                  "Connection: close\r\n"
                  "Metadata: true\r\n"
                  "Accept: application/json\r\n"
                  "\r\n");
   bson_string_free (req_str, true);
}

static const char *
_get_test_imds_host (void)
{
   return getenv ("MCD_TEST_AZURE_IMDS_HOST");
}

static void
_test_with_mock_server (void *ctx)
{
   BSON_UNUSED (ctx);

   bson_error_t error = {0};
   struct _mongoc_host_list_t host;
   _mongoc_host_list_from_string_with_err (
      &host, _get_test_imds_host (), &error);
   ASSERT_ERROR_CONTAINS (error, 0, 0, "");

   mcd_azure_access_token token = {0};
   mcd_azure_access_token_from_imds (&token, host.host, host.port, &error);
   ASSERT_ERROR_CONTAINS (error, 0, 0, "");
   mcd_azure_access_token_destroy (&token);
}

static int
have_mock_server_env (TestSuite *ctx)
{
   return _get_test_imds_host () != NULL;
}

void
test_mcd_azure_imds_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/azure/imds/http/parse", _test_oauth_parse);
   TestSuite_Add (suite, "/azure/imds/http/request", _test_http_req);
   TestSuite_AddFull (suite,
                      "/azure/imds/http/talk",
                      _test_with_mock_server,
                      NULL,
                      NULL,
                      have_mock_server_env);
}
