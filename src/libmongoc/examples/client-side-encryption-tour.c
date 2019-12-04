#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   char *local_masterkey_hex;
   bson_t *kms_providers;
   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   bson_error_t error;
   bson_t *to_insert;

   mongoc_init ();

   local_masterkey_hex = getenv ("LOCAL_MASTERKEY");
   if (!local_masterkey_hex || strlen (local_masterkey_hex) != 96 * 2) {
      fprintf (stderr, "Specify LOCAL_MASTERKEY environment variable as a "
                       "secure random 96 byte hex value.\n");
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   kms_providers = BCON_NEW ("local",
                             "{",
                             "key",
                             BCON_BIN (0, (uint8_t *) local_masterkey_hex, 96),
                             "}");
   auto_encryption_opts = mongoc_auto_encryption_opts_new ();
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts,
                                                  kms_providers);
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      auto_encryption_opts, "admin", "datakeys");

   client = mongoc_client_new (
      "mongodb://localhost/?appname=example-client-side-encryption");
   mongoc_client_set_error_api (client, MONGOC_ERROR_API_VERSION_2);
   if (!mongoc_client_enable_auto_encryption (
          client, auto_encryption_opts, &error)) {
      fprintf (
         stderr, "Error enabling client side encryption: %s\n", error.message);
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   coll = mongoc_client_get_collection (client, "test", "coll");
   to_insert = BCON_NEW ("encryptedField", "123456789");
   if (!mongoc_collection_insert_one (
          coll, to_insert, NULL /* opts */, NULL /* reply */, &error)) {
      fprintf (stderr, "Error inserting document: %s\n", error.message);
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   bson_destroy (to_insert);
   mongoc_collection_destroy (coll);
   bson_destroy (kms_providers);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
   return EXIT_SUCCESS;
}
