#include <mongoc.h>
#include <stdio.h>


int
main (int   argc,
      char *argv[])
{
   mongoc_database_t *database;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *reply;
   bson_uint16_t port;
   bson_error_t error;
   bson_t ping;
   char *host_and_port;
   char *str;

   if (argc < 2 || argc > 3) {
      fprintf(stderr, "usage: %s HOSTNAME [PORT]\n", argv[0]);
      return 1;
   }

   port = (argc == 3) ? atoi(argv[2]) : 27017;
   host_and_port = bson_strdup_printf("mongodb://%s:%hu", argv[1], port);
   client = mongoc_client_new(host_and_port);
   if (!client) {
      fprintf(stderr, "Invalid hostname or port: %s\n", host_and_port);
      return 2;
   }

   bson_init(&ping);
   bson_append_int32(&ping, "ping", 4, 1);
   database = mongoc_client_get_database(client, "test");
   cursor = mongoc_database_command(database, 0, 0, 1, &ping, NULL, NULL);
   if (mongoc_cursor_next(cursor, &reply)) {
      str = bson_as_json(reply, NULL);
      fprintf(stdout, "%s\n", str);
      bson_free(str);
   } else if (mongoc_cursor_error(cursor, &error)) {
      fprintf(stderr, "Ping failure: %s\n", error.message);
      bson_error_destroy(&error);
      return 3;
   }

   mongoc_cursor_destroy(cursor);
   bson_destroy(&ping);
   mongoc_client_destroy(client);
   bson_free(host_and_port);

   return 0;
}
