// example-manage-search-index creates, lists, updates, and deletes an Atlas
// search index from the `test.test` collection.

#include <mongoc/mongoc.h>
#include <stdlib.h> // abort

#define HANDLE_ERROR(...)            \
   if (1) {                          \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n");        \
      goto fail;                     \
   } else                            \
      (void) 0

#define ASSERT(stmt)                                            \
   if (!stmt) {                                                 \
      fprintf (stderr,                                          \
               "assertion failed on line: %d, statement: %s\n", \
               __LINE__,                                        \
               #stmt);                                          \
      abort ();                                                 \
   } else                                                       \
      (void) 0

int
main (int argc, char *argv[])
{
   mongoc_client_t *client = NULL;
   const char *uri_string =
      "mongodb://127.0.0.1/?appname=create-search-indexes-example";
   mongoc_uri_t *uri = NULL;
   mongoc_collection_t *coll = NULL;
   bson_error_t error;
   bool ok = false;

   mongoc_init ();

   if (argc > 2) {
      HANDLE_ERROR (
         "Unexpected arguments. Expected usage: %s [CONNECTION_STRING]",
         argv[0]);
   }

   if (argc > 1) {
      uri_string = argv[1];
   }

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      HANDLE_ERROR ("Failed to parse URI: %s", error.message);
   }
   client = mongoc_client_new_from_uri_with_error (uri, &error);
   if (!client) {
      HANDLE_ERROR ("Failed to create client: %s", error.message);
   }

   coll = mongoc_client_get_collection (client, "test", "test");

   {
      // Create an Atlas Search Index ... begin
      const char *cmd_str = BSON_STR ({
         "createSearchIndexes" : "test",
         "indexes" : [ {
            "definition" : {"mappings" : {"dynamic" : true}},
            "name" : "test index"
         } ]
      });
      bson_t cmd;
      ASSERT (bson_init_from_json (&cmd, cmd_str, -1, &error));
      if (!mongoc_collection_command_simple (
             coll, &cmd, NULL /* read_prefs */, NULL /* reply */, &error)) {
         bson_destroy (&cmd);
         HANDLE_ERROR ("Failed to run createSearchIndexes: %s", error.message);
      }
      printf ("Created index: \"test index\"\n");
      bson_destroy (&cmd);
      // Create an Atlas Search Index ... end
   }

   {
      // List Atlas Search Indexes ... begin
      const char *pipeline_str =
         BSON_STR ({"pipeline" : [ {"$listSearchIndexes" : {}} ]});
      bson_t pipeline;
      ASSERT (bson_init_from_json (&pipeline, pipeline_str, -1, &error));
      mongoc_cursor_t *cursor =
         mongoc_collection_aggregate (coll,
                                      MONGOC_QUERY_NONE,
                                      &pipeline,
                                      NULL /* opts */,
                                      NULL /* read_prefs */);
      printf ("Listing indexes:\n");
      const bson_t *got;
      while (mongoc_cursor_next (cursor, &got)) {
         char *got_str = bson_as_canonical_extended_json (got, NULL);
         printf ("  %s\n", got_str);
         bson_free (got_str);
      }
      if (mongoc_cursor_error (cursor, &error)) {
         bson_destroy (&pipeline);
         mongoc_cursor_destroy (cursor);
         HANDLE_ERROR ("Failed to run $listSearchIndexes: %s", error.message);
      }
      bson_destroy (&pipeline);
      mongoc_cursor_destroy (cursor);
      // List Atlas Search Indexes ... end
   }

   {
      // Update an Atlas Search Index ... begin
      const char *cmd_str = BSON_STR ({
         "updateSearchIndex" : "test",
         "definition" : {},
         "name" : "test index"
      });
      bson_t cmd;
      ASSERT (bson_init_from_json (&cmd, cmd_str, -1, &error));
      if (!mongoc_collection_command_simple (
             coll, &cmd, NULL /* read_prefs */, NULL /* reply */, &error)) {
         bson_destroy (&cmd);
         HANDLE_ERROR ("Failed to run updateSearchIndex: %s", error.message);
      }
      printf ("Updated index: \"test index\"\n");
      bson_destroy (&cmd);
      // Update an Atlas Search Index ... end
   }

   {
      // Drop an Atlas Search Index ... begin
      const char *cmd_str =
         BSON_STR ({"dropSearchIndex" : "test", "name" : "test index"});
      bson_t cmd;
      ASSERT (bson_init_from_json (&cmd, cmd_str, -1, &error));
      if (!mongoc_collection_command_simple (
             coll, &cmd, NULL /* read_prefs */, NULL /* reply */, &error)) {
         bson_destroy (&cmd);
         HANDLE_ERROR ("Failed to run dropSearchIndex: %s", error.message);
      }
      printf ("Dropped index: \"test index\"\n");
      bson_destroy (&cmd);
      // Drop an Atlas Search Index ... end
   }

   ok = true;
fail:
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mongoc_cleanup ();
   return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
