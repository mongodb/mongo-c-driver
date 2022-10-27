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

   if (!mongodb_uri) {
      MONGOC_ERROR (
         "Error: expecting MONGODB_URI environment variable to be set.");
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
                                 BCON_UTF8 ("key-ring-csfle"),
                                 "keyName",
                                 BCON_UTF8 ("key-name-csfle"),
                                 "location",
                                 BCON_UTF8 ("global"),
                                 "projectId",
                                 BCON_UTF8 ("devprod-drivers"));
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
