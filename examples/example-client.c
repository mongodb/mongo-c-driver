/* gcc example-client.c -o example-client $(pkg-config --cflags --libs
 * libmongoc-1.0) */

/* ./example-client [CONNECTION_STRING [COLLECTION_NAME]] */

#include <mongoc.h>
#include <stdio.h>
#include <stdlib.h>


void
run_query (const char *uri_str, const bson_t *query)
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   const bson_t *reply;
   char *str;

   client = mongoc_client_new (uri_str);

   mongoc_client_set_error_api (client, 2);

   collection = mongoc_client_get_collection (client, "db", "collection");
   cursor = mongoc_collection_find_with_opts (
      collection,
      query,
      NULL,  /* additional options */
      NULL); /* read prefs, NULL for default */

   /* this loop is never run: mongoc_cursor_next immediately returns false */
   while (mongoc_cursor_next (cursor, &doc)) {
   }

   if (mongoc_cursor_error_document (cursor, &error, &reply)) {
      str = bson_as_json (reply, NULL);
      fprintf (stderr, "Cursor Failure: %s\nReply: %s\n", error.message, str);
      bson_free (str);
   }

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


int
main (int argc, char *argv[])
{
   bson_t *good_query;
   bson_t *bad_query;

   mongoc_init ();

   /* find documents matching the query {"x": 1} */
   good_query = BCON_NEW ("x", BCON_INT64 (1));

   /* Cause a network error. This will print an error and empty reply document:
    *
    * Cursor Failure: No suitable servers found (`serverSelectionTryOnce` set):
    *     [Failed to resolve 'fake-domain']
    *
    * Reply: { }
    *
    */
   run_query ("mongodb://fake-domain/?appname=cursor-example", good_query);

   /* invalid: {"x": {"$badOperator": 1}} */
   bad_query = BCON_NEW ("x", "{", "$badOperator", BCON_INT64 (1), "}");

   /* Cause a server error. This will print an error and server reply document:
    * 
    * Cursor Failure: unknown operator: $badOperator
    *
    * Reply: 
    * {"ok": 0.0,
    *  "errmsg":"unknown operator: $badOperator",
    *  "code": 2,
    *  "codeName":"BadValue"
    * }
    *
    */
   run_query ("mongodb://localhost/?appname=cursor-example", bad_query);

   bson_destroy (good_query);
   bson_destroy (bad_query);

   mongoc_cleanup ();

   return 0;
}
