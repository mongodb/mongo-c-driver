#include <mongoc/mongoc.h>
#include "TestSuite.h"
#include "test-libmongoc.h"
#include "mongoc/mongoc.h"
#include "mongoc/mongoc-http-private.h"
#include "mongoc/mongoc-uri.h"


void
test_gcpkms (void)
{
   bson_error_t error;
   char *mongodb_uri = getenv ("MONGODB_URI");
   // char *expect_error = getenv ("EXPECT_ERROR");
   char *keyName = getenv ("KEY_NAME");
   char *keyRing = getenv ("KEY_RING");
   char *location = getenv ("LOCATION");
   char *projectId = getenv ("PROJECT_ID");

   if (!mongodb_uri) {
      MONGOC_ERROR ("Error: expecting environment variables to be set: "
                    "MONGODB_URI, KEY_NAME, KEY_VAULT_ENDPOINT");
      // return EXIT_FAILURE;
   }

   mongoc_init ();
   mongoc_client_t *keyvault_client = mongoc_client_new (mongodb_uri);
   MONGOC_DEBUG ("libmongoc version: %s", mongoc_get_version ());

   mongoc_client_encryption_t *ce;
   mongoc_client_encryption_opts_t *ceopts;

   ceopts = mongoc_client_encryption_opts_new ();
   mongoc_client_encryption_opts_set_keyvault_client (ceopts, keyvault_client);
   mongoc_client_encryption_opts_set_keyvault_namespace (
      ceopts, "keyvault", "datakeys");

   bson_t *kms_providers = BCON_NEW ("gcp", "{", "}");
   mongoc_client_encryption_opts_set_kms_providers (ceopts, kms_providers);
   ce = mongoc_client_encryption_new (ceopts, &error);

   if (!ce) {
      MONGOC_ERROR ("Error in mongoc_client_encryption_new: %s", error.message);
   }

   mongoc_client_encryption_datakey_opts_t *dkopts;
   dkopts = mongoc_client_encryption_datakey_opts_new ();
   bson_t *masterkey = BCON_NEW ("keyRing",
                                 BCON_UTF8 (keyRing),
                                 "keyName",
                                 BCON_UTF8 (keyName),
                                 "location",
                                 BCON_UTF8 (location),
                                 "projectId",
                                 BCON_UTF8 (projectId));
   mongoc_client_encryption_datakey_opts_set_masterkey (dkopts, masterkey);

   bson_value_t keyid;
   bool got = mongoc_client_encryption_create_datakey (
      ce, "gcp", dkopts, &keyid, &error);
   if (!got) {
      MONGOC_ERROR ("Expected to create data key, but got error: %s",
                    error.message);
   }
   MONGOC_DEBUG ("Created key\n");

   bson_value_destroy (&keyid);
   bson_destroy (masterkey);
   mongoc_client_encryption_datakey_opts_destroy (dkopts);
   mongoc_client_encryption_destroy (ce);
   bson_destroy (kms_providers);
   mongoc_client_encryption_opts_destroy (ceopts);
   mongoc_client_destroy (keyvault_client);
   mongoc_cleanup ();
}

void
test_gcp_kms_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/test_gcpkms", test_gcpkms);
}

// mongoc_http_request_t req;
// mongoc_http_response_t res;
// bool r;
// BSON_UNUSED (unused);

// _mongoc_http_request_init (&req);
// _mongoc_http_response_init (&res);

/* Basic GET request */
// req.method = "GET";
// req.host = "localhost";
// req.port = 5000;
// // Empty body is okay
// req.body = "";
// req.body_len = 0;
// r = _mongoc_http_send (&req, 10000, false, NULL, &res, &error);
// ASSERT_CMPINT (res.status, ==, 200);
// ASSERT_OR_PRINT (r, error);
// ASSERT_CMPINT (res.body_len, >, 0);
// _mongoc_http_response_cleanup (&res);