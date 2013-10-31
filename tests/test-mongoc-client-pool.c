#include <mongoc.h>

#include "mongoc-tests.h"


static void
test_mongoc_client_pool_basic (void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new("mongodb://127.0.0.1?maxpoolsize=1&minpoolsize=1");
   pool = mongoc_client_pool_new(uri);
   client = mongoc_client_pool_pop(pool);
   assert(client);
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}


static void
test_mongoc_client_pool_try_pop (void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   uri = mongoc_uri_new("mongodb://127.0.0.1?maxpoolsize=1&minpoolsize=1");
   pool = mongoc_client_pool_new(uri);
   client = mongoc_client_pool_pop(pool);
   assert(client);
   assert(!mongoc_client_pool_try_pop(pool));
   mongoc_client_pool_push(pool, client);
   mongoc_uri_destroy(uri);
   mongoc_client_pool_destroy(pool);
}


static void
log_handler (mongoc_log_level_t  log_level,
             const char         *domain,
             const char         *message,
             void               *user_data)
{
   /* Do Nothing */
}


int
main (int   argc,
      char *argv[])
{
   if (argc <= 1 || !!strcmp(argv[1], "-v")) {
      mongoc_log_set_handler(log_handler, NULL);
   }

   run_test("/mongoc/client/pool/basic", test_mongoc_client_pool_basic);
   run_test("/mongoc/client/pool/try_pop", test_mongoc_client_pool_try_pop);

   return 0;
}
