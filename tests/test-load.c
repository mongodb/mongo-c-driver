#include <mongoc.h>
#include <mongoc-client-private.h>
#include <mongoc-event-private.h>


static void
test_load (mongoc_client_t *client,
           unsigned         iterations)
{
   mongoc_event_t ev;
   bson_uint32_t hint;
   bson_error_t error;
   unsigned i;
   bson_t b;
   bson_t f;

   bson_init(&f);

   bson_init(&b);
   bson_append_int32(&b, "ping", 4, 1);
   bson_destroy(&b);

   for (i = 0; i < iterations; i++) {
      memset(&error, 0, sizeof error);
      memset(&ev, 0, sizeof ev);
      ev.any.type = MONGOC_OPCODE_QUERY;
      ev.any.opcode = MONGOC_OPCODE_QUERY;
      ev.query.flags = 0;
      ev.query.ns = "admin.$cmd";
      ev.query.nslen = 10;
      ev.query.skip = 0;
      ev.query.n_return = 1;
      ev.query.query = &b;
      ev.query.fields = NULL;
      if (!(hint = mongoc_client_send(client, &ev, 0, &error))) {
         MONGOC_DEBUG("Send failed: %s", error.message);
         bson_error_destroy(&error);
         assert(FALSE);
      }
      if (!mongoc_client_recv(client, &ev, hint, &error)) {
         MONGOC_DEBUG("Recv failed: %s", error.message);
         bson_error_destroy(&error);
         assert(FALSE);
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
