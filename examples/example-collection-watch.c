#include <mongoc.h>

int
main ()
{
   bson_t empty = BSON_INITIALIZER;
   const bson_t *doc;
   bson_t *to_insert = BCON_NEW ("x", BCON_INT32 (1));
   const bson_t *err_doc;
   bson_error_t err;
   mongoc_client_t *client;
   mongoc_collection_t *coll;
   mongoc_change_stream_t *stream;
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();

   mongoc_init ();

   client = mongoc_client_new ("mongodb://"
                               "localhost:27017,localhost:27018,localhost:"
                               "27019/db?replicaSet=rs0");
   if (!client) {
      printf ("Could not connect to replica set\n");
      return 1;
   }

   coll = mongoc_client_get_collection (client, "db", "coll");
   stream = mongoc_collection_watch (coll, &empty, NULL);

   mongoc_write_concern_set_wmajority (wc, 1000);
   mongoc_collection_insert (coll, MONGOC_INSERT_NONE, to_insert, wc, NULL);

   while (mongoc_change_stream_next (stream, &doc)) {
      char *as_json = bson_as_relaxed_extended_json (doc, NULL);
      printf ("Got document: %s\n", as_json);
      bson_free (as_json);
   }

   if (mongoc_change_stream_error_document (stream, &err, &err_doc)) {
      if (!bson_empty (err_doc)) {
         printf ("Server Error: %s\n",
                 bson_as_relaxed_extended_json (err_doc, NULL));
      } else {
         printf ("Client Error: %s\n", err.message);
      }
      return 1;
   }

   bson_destroy (to_insert);
   mongoc_write_concern_destroy (wc);
   mongoc_change_stream_destroy (stream);
   mongoc_collection_destroy (coll);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
}