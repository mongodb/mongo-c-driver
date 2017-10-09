/* gcc example-session.c -o example-session \
 *     $(pkg-config --cflags --libs libmongoc-1.0) */

/* ./example-session [CONNECTION_STRING] */

#include <stdio.h>
#include <mongoc.h>


int
main (int argc, char *argv[])
{
   int exit_code = EXIT_FAILURE;

   mongoc_client_t *client;
   mongoc_session_opt_t *opts = NULL;
   mongoc_client_session_t *session = NULL;
   mongoc_collection_t *collection = NULL;
   const char *uristr = "mongodb://127.0.0.1/?appname=session-example";
   bson_error_t error;
   bson_t *filter;
   bson_t *update;
   bson_t *find_opts;
   mongoc_read_prefs_t *secondary = NULL;
   mongoc_cursor_t *cursor = NULL;
   const bson_t *doc;
   char *str;
   bool r;

   mongoc_init ();

   if (argc > 1) {
      uristr = argv[1];
   }

   client = mongoc_client_new (uristr);

   if (!client) {
      fprintf (stderr, "Failed to parse URI.\n");
      goto done;
   }

   mongoc_client_set_error_api (client, 2);

   opts = mongoc_session_opts_new ();
   mongoc_session_opts_set_retry_writes (opts, true);
   mongoc_session_opts_set_causal_consistency (opts, true);
   session = mongoc_client_start_session (client, opts, &error);
   mongoc_session_opts_destroy (opts);

   if (!session) {
      fprintf (stderr, "Failed to start session: %s\n", error.message);
      goto done;
   }

   /* create a collection bound to the session */
   collection = mongoc_client_session_get_collection (session, "db", "collection");
   filter = BCON_NEW ("_id", BCON_INT32 (1));
   update = BCON_NEW ("$inc", "{", "x", BCON_INT32 (1), "}");

   /*
    * update with "$inc". since we're in a retry-writes session, the update is
    * safely retried once if there's a network error
    */
   r = mongoc_collection_update (collection,
                                 MONGOC_UPDATE_UPSERT,
                                 filter,
                                 update,
                                 NULL /* write concern */,
                                 &error);

   bson_destroy (filter);
   bson_destroy (update);

   if (!r) {
      fprintf (stderr, "Update failed: %s\n", error.message);
      goto done;
   }

   filter = BCON_NEW ("_id", BCON_INT32 (1));
   secondary = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   find_opts = BCON_NEW ("maxTimeMS", BCON_INT32 (2000));

   /* read from secondary. since we're in a causally consistent session, the
    * data is guaranteed to reflect the update we did on the primary. the query
    * blocks waiting for the secondary to catch up, if necessary, or times out
    * and fails after 2000 ms.
    */
   cursor = mongoc_collection_find_with_opts (
      collection, filter, find_opts, secondary);

   bson_destroy (filter);
   mongoc_read_prefs_destroy (secondary);
   bson_destroy (find_opts);

   while (mongoc_cursor_next (cursor, &doc)) {
      str = bson_as_json (doc, NULL);
      fprintf (stdout, "%s\n", str);
      bson_free (str);
   }

   if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "Cursor Failure: %s\n", error.message);
      goto done;
   }

   exit_code = EXIT_SUCCESS;

done:
   /* must destroy cursor and collection before the session they came from */
   if (cursor) {
      mongoc_cursor_destroy (cursor);
   }
   if (collection) {
      mongoc_collection_destroy (collection);
   }
   if (session) {
      mongoc_client_session_destroy (session);
   }

   /* finish cleaning up */
   if (client) {
      mongoc_client_destroy (client);
   }

   mongoc_cleanup ();

   return exit_code;
}
