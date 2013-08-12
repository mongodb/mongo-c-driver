#include <mongoc.h>

#include "mongoc-tests.h"


#define MONGOC_TEST_URI "mongodb://testuser:testpass@localhost:27017/?authSource=auth"


static void
test_mongoc_client_authenticate (void)
{
   mongoc_database_t *database;
   mongoc_client_t *client;

   client = mongoc_client_new(MONGOC_TEST_URI);
   database = mongoc_client_get_database(client, "auth");

   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
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

   run_test("/mongoc/client/authenticate", test_mongoc_client_authenticate);

   return 0;
}
