#include <mongoc.h>
#include <mongoc-conn-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_conn_init_tcp (void)
{
   mongoc_conn_t conn;
   bson_error_t error;

   mongoc_conn_init_tcp(&conn, "127.0.0.1", 27017, NULL);
   assert(mongoc_conn_connect(&conn, &error));
   mongoc_conn_destroy(&conn);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/conn/init_tcp", test_mongoc_conn_init_tcp);

   return 0;
}
