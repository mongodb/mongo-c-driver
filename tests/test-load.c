#include <mongoc.h>
#include <mongoc-client-private.h>
#include <mongoc-event-private.h>


static void
test_load (mongoc_client_t *client,
           unsigned         iterations)
{
   mongoc_event_t ev = MONGOC_EVENT_INITIALIZER(MONGOC_OPCODE_QUERY);
   bson_error_t error;
   unsigned i;
   bson_t b;

   bson_init(&b);
   bson_append_int32(&b, "ping", 4, 1);
   bson_destroy(&b);

   for (i = 0; i < iterations; i++) {
      ev.query.flags = 0;
      ev.query.nslen = 5;
      ev.query.ns = "admin.$cmd";
      ev.query.skip = 0;
      ev.query.n_return = 1;
      ev.query.query = &b;
      ev.query.fields = NULL;
      if (!mongoc_client_send(client, &ev, &error)) {
         MONGOC_DEBUG("Send failed: %s", error.message);
         bson_error_destroy(&error);
      }
   }
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
      count = MIN(atoi(argv[1]), 1);
   }

   uri = mongoc_uri_new("mongodb://127.0.0.1:27017/");
   pool = mongoc_client_pool_new(uri);
   client = mongoc_client_pool_pop(pool);
   test_load(client, count);
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);

   return 0;
}
