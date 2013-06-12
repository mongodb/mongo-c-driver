#include <mongoc.h>
#include <mongoc-client-private.h>
#include <mongoc-event-private.h>


static void
test_load (mongoc_client_t *client,
           unsigned         iterations)
{
   mongoc_database_t *database;
   mongoc_cursor_t *cursor;

   bson_error_t error;
   const bson_t *bp;
   unsigned i;
   bson_t b;
   bson_t f;

   bson_init(&f);

   bson_init(&b);
   bson_append_int32(&b, "ping", 4, 1);
   bson_destroy(&b);

   database = mongoc_client_get_database(client, "admin");

   for (i = 0; i < iterations; i++) {
      memset(&error, 0, sizeof error);
      cursor = mongoc_database_command(database,
                                       MONGOC_QUERY_NONE,
                                       0,
                                       1,
                                       &b,
                                       &f,
                                       NULL,
                                       &error);
      if (!cursor) {
         MONGOC_DEBUG("Command failed: %s", error.message);
         bson_error_destroy(&error);
         continue;
      }

      while ((bp = mongoc_cursor_next(cursor))) {
         char *str;

         str = bson_as_json(bp, NULL);
         MONGOC_DEBUG("%d: %s", i, str);
         bson_free(str);
      }

#if 0
      if (bson_cursor_is_error(cursor)) {
      }
#endif

      mongoc_cursor_destroy(cursor);
   }

   mongoc_database_destroy(database);
}


int
main (int   argc,
      char *argv[])
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;
   unsigned count = 10000;

   if (argc > 1) {
      if (!(uri = mongoc_uri_new(argv[1]))) {
         fprintf(stderr, "Failed to parse uri: %s\n", argv[1]);
         return 1;
      }
   } else {
      uri = mongoc_uri_new("mongodb://127.0.0.1:27017/");
   }

   if (argc > 2) {
      count = MAX(atoi(argv[2]), 1);
   }

   pool = mongoc_client_pool_new(uri);
   client = mongoc_client_pool_pop(pool);
   test_load(client, count);
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);

   return 0;
}
