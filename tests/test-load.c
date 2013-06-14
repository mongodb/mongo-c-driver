#include <mongoc.h>
#include <mongoc-client-private.h>
#include <mongoc-event-private.h>


static void
print_doc (const bson_t *b)
{
   char *str;

   str = bson_as_json(b, NULL);
   MONGOC_DEBUG("%s", str);
   bson_free(str);
}


static void
ping (mongoc_database_t *db,
      bson_t            *cmd)
{
   mongoc_cursor_t *cursor;
   const bson_t *b;
   bson_error_t error;

   cursor = mongoc_database_command(db, MONGOC_QUERY_NONE, 0, 1, cmd, NULL, NULL, NULL);
   while (mongoc_cursor_next(cursor, &b)) {
      BSON_ASSERT(b);
      print_doc(b);
   }
   if (mongoc_cursor_error(cursor, &error)) {
      MONGOC_WARNING("Cursor error: %s", error.message);
      bson_error_destroy(&error);
   }
   mongoc_cursor_destroy(cursor);
}


static void
test_load (mongoc_client_t *client,
           unsigned         iterations)
{
   mongoc_database_t *db;
   unsigned i;
   bson_t b;

   bson_init(&b);
   bson_append_int32(&b, "ping", 4, 1);

   db = mongoc_client_get_database(client, "admin");

   for (i = 0; i < iterations; i++) {
      ping(db, &b);
   }

   mongoc_database_destroy(db);
   bson_destroy(&b);
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
