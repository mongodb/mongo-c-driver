/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

static void
command_started (const mongoc_apm_command_started_t *event)
{
   char *s;

   s = bson_as_relaxed_extended_json (
      mongoc_apm_command_started_get_command (event), NULL);
   printf ("Command %s started on %s:\n%s\n\n",
           mongoc_apm_command_started_get_command_name (event),
           mongoc_apm_command_started_get_host (event)->host,
           s);

   bson_free (s);
}

static void example_listcollections (mongoc_client_t *client) {
   mongoc_database_t *db;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_t* opts = BCON_NEW ("cursor", "{", "batchSize", BCON_INT32(1), "}");
   char *str;
   bson_error_t error;

   MONGOC_DEBUG ("Example listCollections");
   MONGOC_DEBUG ("listCollections with the option {cursor: {batchSize: 1}}");
   db = mongoc_client_get_database(client, "test");
   cursor = mongoc_database_find_collections_with_opts (db, opts);

   MONGOC_DEBUG ("listCollections results:");
   while (mongoc_cursor_next (cursor, &doc)) {
      str = bson_as_canonical_extended_json (doc, NULL);
      fprintf (stdout, "- %s\n", str);
      bson_free (str);
   }

   if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "listCollections error: %s\n", error.message);
      return;
   }
   mongoc_cursor_destroy (cursor);
   mongoc_database_destroy (db);
   bson_destroy (opts);
}

static void example_listindexes (mongoc_client_t *client) {
   mongoc_collection_t *coll;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_t* opts = BCON_NEW ("cursor", "{", "batchSize", BCON_INT32(1), "}");
   char *str;
   bson_error_t error;

   MONGOC_DEBUG ("Example listIndexes");
   MONGOC_DEBUG ("listIndexes with the option {cursor: {batchSize: 1}}");
   coll = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find_indexes_with_opts (coll, opts);

   MONGOC_DEBUG ("listIndexes results:");
   while (mongoc_cursor_next (cursor, &doc)) {
      str = bson_as_canonical_extended_json (doc, NULL);
      fprintf (stdout, "- %s\n", str);
      bson_free (str);
   }

   if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "listCollections error: %s\n", error.message);
      return;
   }
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (coll);
   bson_destroy (opts);
}

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   const char *uri_string = "mongodb://127.0.0.1/?appname=client-example";
   mongoc_apm_callbacks_t *callbacks;

   mongoc_init ();
   if (argc > 1) {
      uri_string = argv[1];
   }

   client = mongoc_client_new (uri_string);
   if (!client) {
      return EXIT_FAILURE;
   }

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, command_started);
   mongoc_client_set_apm_callbacks (client, callbacks, NULL);
   mongoc_client_set_error_api (client, 2);

   // example_listcollections (client);
   example_listindexes (client);

   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}
