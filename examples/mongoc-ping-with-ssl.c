/*
 * Copyright 2016 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 *
 * Run the server with SSL enabled:
 * $ mongod --sslMode requireSSL --sslPEMKeyFile tests/x509gen/server.pem --sslCAFile tests/x509gen/ca.pem
 *
 * To connect to the database in the shell:
 * $ mongo --ssl --sslPEMKeyFile tests/x509gen/client.pem --sslCAFile tests/x509gen/ca.pem --host localhost
 *
 * Compile this file:
 * $ gcc -o ping mongoc-ping-with-ssl.c $(pkg-config --cflags --libs libmongoc-1.0)
 *
 * and run the executable:
 * $ ./ping localhost
 *
 */


#include <mongoc.h>
#include <stdio.h>


int
main (int   argc,
      char *argv[])
{
   mongoc_cursor_t *cursor = NULL;
   mongoc_cursor_t *cursor_find = NULL;
   mongoc_client_t *client = NULL;
   mongoc_database_t *database = NULL;
   mongoc_collection_t *collection = NULL;
   const bson_t *reply = NULL;
   uint16_t port;
   bson_t *query = NULL;
   bson_error_t error;
   bson_t ping = BSON_INITIALIZER;
   bson_t *insert = NULL;
   const bson_t *doc = NULL;
   char *host_and_port = NULL;
   char *str = NULL;
   const char* collection_name = "collection";
   const char* db_name = "db";
   int res = 0;

   mongoc_ssl_opt_t ssl_opts = { 0 };

   if (argc < 2 || argc > 3) {
      fprintf (stderr, "usage: %s <connection string> [port]\n",
               argv[0]);
      fprintf (stderr,
               "the connection string can be of the following forms:\n");
      fprintf (stderr, "localhost\t\t\t\tlocal machine\n");
      fprintf (stderr,
               "mongodb://localhost:27018\t\tlocal machine on port 27018 "
               "(could also use [port] argument)\n");
      fprintf (stderr,
               "mongodb://user:pass@localhost:27017\t"
               "local machine on port 27017, and authenticate with username "
               "user and password pass\n");
      return 1;
   }

   mongoc_init ();

   port = (argc == 3) ? atoi (argv[2]) : 27017;

   if (strncmp (argv[1], "mongodb://", 10) == 0) {
      host_and_port = bson_strdup (argv [1]);
   } else {
      host_and_port = bson_strdup_printf ("mongodb://%s:%hu", argv[1], port);
   }

   client = mongoc_client_new (host_and_port);
   if (!client) {
      fprintf (stderr, "Invalid hostname or port: %s\n", host_and_port);
      res = 2;
      goto cleanup;
   }

   database = mongoc_client_get_database (client, db_name);
   collection = mongoc_client_get_collection (client, db_name,
                                              collection_name);

   ssl_opts.pem_file = "../tests/x509gen/client.pem";
   ssl_opts.ca_file = "../tests/x509gen/ca.pem";
   ssl_opts.weak_cert_validation = false;
   mongoc_client_set_ssl_opts (client, &ssl_opts);

   /* ping */

   bson_init (&ping);
   bson_append_int32 (&ping, "ping", 4, 1);
   database = mongoc_client_get_database (client, db_name);
   cursor = mongoc_database_command (database, (mongoc_query_flags_t)0, 0, 1,
                                     0, &ping, NULL, NULL);
   if (mongoc_cursor_next (cursor, &reply)) {
      str = bson_as_json (reply, NULL);
      fprintf (stdout, "%s\n", str);
      bson_free (str);
   } else if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "Ping failure: %s\n", error.message);
      res = 3;
      goto cleanup;
   }

   /* insert document */

   insert = BCON_NEW (
      "name", "{",
         "first_name", "judas",
         "last_name", "smith",
      "}",
      "city", "New York",
      "state", "New York",
      "favorite color", "green",
      "zip", BCON_INT32 (11201),
      "age", BCON_INT32 (65)
      );

   if (!mongoc_collection_insert (collection, MONGOC_INSERT_NONE, insert,
                                  NULL, &error)) {
      fprintf (stderr, "Couldn't insert doc to %s: %s\n", collection_name,
               error.message);
      res = 4;
      goto cleanup;
   }

   /* query all documents from db.collection */

   query = BCON_NEW ("name.first_name", "judas",
                     "favorite color", "green");

   cursor_find = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0,
                                         0, query, NULL, NULL);
   while (mongoc_cursor_next (cursor_find, &doc)) {
      str = bson_as_json (doc, NULL);
      printf ("%s\n", str);
      bson_free (str);
   }

cleanup:
   if (query) {
      bson_destroy (query);
   }
   if (cursor_find) {
      mongoc_cursor_destroy (cursor_find);
   }

   if (insert) {
      bson_destroy (insert);
   }

   if (collection) {
      mongoc_collection_destroy (collection);
   }

   if (database) {
      mongoc_database_destroy (database);
   }

   if (cursor) {
      mongoc_cursor_destroy (cursor);
   }

   bson_destroy (&ping);

   if (client) {
      mongoc_client_destroy (client);
   }

   if (host_and_port) {
      bson_free (host_and_port);
   }

   mongoc_cleanup ();

   return res;
}
