/*
 * Used as a benchmark for CDRIVER-4656 to test the performance effect of sharing the OpenSSL context among all
 * connections made by a client.
 *
 * TO RUN: % ./cmake-build/src/libmongoc/mongoc-double-pings
 */

#include <mongoc/mongoc.h>
#include <stdio.h>

int
main (int argc, char *argv[])
{
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_database_t *database;
   bool r;

   mongoc_init ();

   // Make URI to create all clients from
   uri = mongoc_uri_new ("mongodb://localhost:27017/");

#ifdef MONGOC_ENABLE_SSL
   const char *certificate_path;
   const char *ca_path;

   // Use built-in test CA and PEM files.
   certificate_path = "./src/libmongoc/tests/x509gen/client.pem";
   ca_path = "./src/libmongoc/tests/x509gen/ca.pem";

   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLS, true);
   mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, certificate_path);
   mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, ca_path);
#endif

   for (int i = 0; i < 5000; i++) {
      bson_error_t error;

      // Create the client
      client = mongoc_client_new_from_uri (uri);

      if (!client) {
         printf ("Client %d failed to initialize\n", i);
         continue;
      }

      // Send 2 pings to the server
      mongoc_client_set_error_api (client, 2);
      database = mongoc_client_get_database (client, "test");

      // First ping
      {
         bson_t ping;
         bson_t reply;

         bson_init (&ping);
         bson_append_int32 (&ping, "ping", 4, 1);

         r = mongoc_database_command_with_opts (database, &ping, NULL, NULL, &reply, &error);

         if (!r) {
            fprintf (stderr, "Ping 1 failure on client %d: %s\n", i, error.message);
         }

         // Cleanup
         bson_destroy (&ping);
         bson_destroy (&reply);
      }

      // Second ping
      {
         bson_t ping;
         bson_t reply;

         bson_init (&ping);
         bson_append_int32 (&ping, "ping", 4, 1);

         r = mongoc_database_command_with_opts (database, &ping, NULL, NULL, &reply, &error);

         if (!r) {
            fprintf (stderr, "Ping 2 failure on client %d: %s\n", i, error.message);
         }

         // Cleanup
         bson_destroy (&ping);
         bson_destroy (&reply);
      }

      mongoc_database_destroy (database);
      mongoc_client_destroy (client);
   }
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();
}
