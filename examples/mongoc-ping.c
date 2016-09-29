/*
 * Copyright 2013-2014 MongoDB, Inc.
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
 */


#include <mongoc.h>
#include <stdio.h>


int
main (int   argc,
      char *argv[])
{
   mongoc_database_t *database;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   uint16_t port = 27017;
   struct stat stat_buf;
   bool use_ssl = false;
   const bson_t *reply;
   char *host_and_port;
   bson_error_t error;
   bson_t ping;
   char *str;
   int opt;
#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t ssl_opts = {0};
#endif

   while ((opt = getopt (argc, argv, "a:p:ds")) != -1) {
      switch (opt) {
         case 'a':
            if (!stat (optarg, &stat_buf) && S_ISREG (stat_buf.st_mode)) {
#ifdef MONGOC_ENABLE_SSL
               ssl_opts.ca_file = optarg;
#endif
               fprintf (stderr, "Verifying certificate against '%s'\n", optarg);
            } else {
               fprintf(stderr, "'%s' is not a readable file\n", optarg);
            }
            break;

         case 'p':
            if (!stat (optarg, &stat_buf) && S_ISREG (stat_buf.st_mode)) {
               fprintf (stderr, "Presenting myself as '%s'\n", optarg);
#ifdef MONGOC_ENABLE_SSL
               ssl_opts.pem_file = optarg;
#endif
            } else {
               fprintf(stderr, "'%s' is not a readable file\n", optarg);
            }
            break;

         case 'd':
            fprintf (stderr, "Disabling hostname verification\n");
#ifdef MONGOC_ENABLE_SSL
            ssl_opts.allow_invalid_hostname = true;
#endif
            break;

         case 's':
            fprintf (stderr, "Enabling SSL\n");
            use_ssl = true;
            break;

         default:
            fprintf (stderr, "Usage:\n\t%s [-a certifcate_authority.pem] "
                  "[-p private_key.pem] [-d] HOSTNAME [PORT]\n"
                  "\t\t(-d disables certificate verification)\n", argv[0]);
            return 2;
      }
   }

   if (optind >= argc) {
      fprintf (stderr, "Usage:\n\t%s [-a certifcate_authority.pem] "
               "[-p private_key.pem] [-d] HOSTNAME [PORT]\n"
               "\t\t(-d disables certificate verification)\n", argv[0]);
      return 2;
   }

   if (strncmp (argv[optind], "mongodb://", 10) == 0) {
      host_and_port = bson_strdup (argv[optind]);
   } else {
      if (optind+2 == argc) {
          port = atoi(argv[optind+1]);
      }
      host_and_port = bson_strdup_printf("mongodb://%s:%hu", argv[optind], port);
   }


   mongoc_init();
   client = mongoc_client_new (host_and_port);
   if (use_ssl) {
#ifdef MONGOC_ENABLE_SSL
      mongoc_client_set_ssl_opts (client, &ssl_opts);
#else
      fprintf (stderr, "Trying to enable SSL when mongoc is compiled without SSL support\n");
      return 2;
#endif
   }

   if (!client) {
      fprintf(stderr, "Invalid hostname or port: %s\n", host_and_port);
      return 2;
   }

   mongoc_client_set_error_api (client, 2);

   bson_init(&ping);
   bson_append_int32(&ping, "ping", 4, 1);
   database = mongoc_client_get_database(client, "test");
   cursor = mongoc_database_command(database, (mongoc_query_flags_t)0, 0, 1, 0, &ping, NULL, NULL);
   if (mongoc_cursor_next(cursor, &reply)) {
      str = bson_as_json(reply, NULL);
      fprintf(stdout, "%s\n", str);
      bson_free(str);
   } else if (mongoc_cursor_error(cursor, &error)) {
      fprintf(stderr, "Ping failure: %s\n", error.message);
      return 3;
   }

   mongoc_cursor_destroy(cursor);
   mongoc_database_destroy (database);
   bson_destroy(&ping);
   mongoc_client_destroy(client);
   bson_free(host_and_port);

   return 0;
}
