#include <mongoc/service-gcp.h>

#include "TestSuite.h"

static void
_test_gcp_parse (void)
{
   // Test that we correctly parse the JSON returned by the GCP metadata server
   bson_error_t error;
   gcp_service_account_token token;

   // out parameter cannot be null

   // server output must be json data
   ASSERT (!gcp_access_token_try_parse_from_json (
      &token, "invalid json", -1, &error));
   ASSERT_CMPUINT32 (error.domain, ==, BSON_ERROR_JSON);

   // server output must contain access_token
   ASSERT (!gcp_access_token_try_parse_from_json (&token, "{}", -1, &error));
   ASSERT_ERROR_CONTAINS (
      error, MONGOC_ERROR_GCP, MONGOC_ERROR_GCP_BAD_JSON, "");

   // server output must contain a value for access_token
   ASSERT (!gcp_access_token_try_parse_from_json (
      &token, BSON_STR ({"access_token" : null}), -1, &error));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_GCP,
                          MONGOC_ERROR_GCP_BAD_JSON,
                          "One or more required JSON");

   // server output must contain token_type
   ASSERT (!gcp_access_token_try_parse_from_json (
      &token, BSON_STR ({"access_token" : "helloworld"}), -1, &error));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_GCP,
                          MONGOC_ERROR_GCP_BAD_JSON,
                          "One or more required JSON");

   // can successfully parse JSON datat into a gcp_service_account_token
   ASSERT (
      gcp_access_token_try_parse_from_json (&token,
                                            BSON_STR ({
                                               "access_token" : "helloworld",
                                               "token_type" : "bearer",
                                               "expires_in" : "3788"
                                            }),
                                            -1,
                                            &error));
   ASSERT_ERROR_CONTAINS (error, 0, 0, "");
   ASSERT_CMPSTR (token.access_token, "helloworld");

   gcp_access_token_destroy (&token);
}


static void
_test_gcp_http_request (void)
{
   // Test that we correctly build a http request for the GCP metadata server
}

// see different responses from the server
void
test_service_gcp_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/gcp/http/parse", _test_gcp_parse);
   TestSuite_Add (suite, "/gcp/http/request", _test_gcp_http_request);
}
