#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   char *local_masterkey_hex;
   bson_t *kms_providers;
   mongoc_client_encryption_t *client_encryption;
   mongoc_client_encryption_opts_t *client_encryption_opts;
   mongoc_client_encryption_datakey_opts_t *datakey_opts;
   mongoc_auto_encryption_opts_t *auto_encryption_opts;
   bson_error_t error;
   bson_value_t keyid;
   bson_t *schema_map;

   mongoc_init ();

   local_masterkey_hex = getenv ("LOCAL_MASTERKEY");
   if (!local_masterkey_hex || strlen (local_masterkey_hex) != 96 * 2) {
      fprintf (stderr,
               "Specify LOCAL_MASTERKEY environment variable as a "
               "secure random 96 byte hex value.\n");
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   kms_providers = BCON_NEW ("local",
                             "{",
                             "key",
                             BCON_BIN (0, (uint8_t *) local_masterkey_hex, 96),
                             "}");


   client = mongoc_client_new (
      "mongodb://localhost/?appname=example-client-side-encryption");
   client_encryption_opts = mongoc_client_encryption_opts_new ();
   mongoc_client_encryption_opts_set_kms_providers (client_encryption_opts,
                                                    kms_providers);
   mongoc_client_encryption_opts_set_keyvault_client (client_encryption_opts,
                                                      client);
   mongoc_client_encryption_opts_set_keyvault_namespace (
      client_encryption_opts, "admin", "datakeys");
   client_encryption =
      mongoc_client_encryption_new (client_encryption_opts, &error);
   if (!client_encryption) {
      fprintf (stderr,
               "Error creating mongoc_client_encryption_t: %s\n",
               error.message);
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   datakey_opts = mongoc_client_encryption_datakey_opts_new ();
   if (!mongoc_client_encryption_create_datakey (
          client_encryption, "local", datakey_opts, &keyid, &error)) {
      fprintf (stderr, "Error creating data key: %s\n", error.message);
      mongoc_cleanup ();
      return EXIT_FAILURE;
   }

   auto_encryption_opts = mongoc_auto_encryption_opts_new ();
   mongoc_auto_encryption_opts_set_kms_providers (auto_encryption_opts,
                                                  kms_providers);
   mongoc_auto_encryption_opts_set_keyvault_namespace (
      auto_encryption_opts, "admin", "datakeys");

   /* Need a schema that references the new data key. */
   schema_map = BCON_NEW ("test.coll",
                          "{",
                          "properties",
                          "{",
                          "encryptedField",
                          "{",
                          "encrypt",
                          "{",
                          "keyId",
                          "[",
                          BCON_BIN (BSON_SUBTYPE_UUID,
                                    keyid.value.v_binary.data,
                                    keyid.value.v_binary.data_len),
                          "]",
                          "bsonType",
                          "string",
                          "algorithm",
                          AEAD_AES_256_CBC_HMAC_SHA_512_DETERMINISTIC,
                          "}",
                          "}",
                          "}",
                          "bsonType",
                          "object",
                          "}");
   mongoc_auto_encryption_opts_set_schema_map (auto_encryption_opts,
                                               schema_map);

   bson_destroy (schema_map);
   bson_value_destroy (&keyid);
   mongoc_client_encryption_datakey_opts_destroy (datakey_opts);
   mongoc_client_encryption_opts_destroy (client_encryption_opts);
   mongoc_client_encryption_destroy (client_encryption);
   mongoc_auto_encryption_opts_destroy (auto_encryption_opts);
   bson_destroy (kms_providers);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
   return EXIT_SUCCESS;
}
