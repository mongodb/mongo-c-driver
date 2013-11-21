#include <fcntl.h>
#include <mongoc.h>
#include <unistd.h>

#include "mongoc-tests.h"
#include "mock-server.h"


#define HOSTENV "MONGOC_TEST_HOST"
#define HOST (getenv(HOSTENV) ? getenv(HOSTENV) : "localhost")


static char *gTestUri;
static char *gTestUriWithPassword;
static char *gTestUriWithBadPassword;


#define MONGOD_VERSION_HEX(a, b, c) ((a << 16) | (b << 8) | (c))


static bson_uint32_t
get_version_hex (mongoc_client_t *client)
{
   mongoc_database_t *database;
   mongoc_cursor_t *cursor;
   bson_uint32_t version = 0;
   const bson_t *doc;
   bson_iter_t child;
   bson_iter_t iter;
   bson_t cmd = BSON_INITIALIZER;
   int i;

   bson_append_int32 (&cmd, "buildInfo", -1, 1);

   database = mongoc_client_get_database (client, "admin");
   cursor = mongoc_database_command (database,
                                     MONGOC_QUERY_NONE,
                                     0,
                                     1,
                                     &cmd,
                                     NULL,
                                     NULL);

   if (mongoc_cursor_next (cursor, &doc)) {
      if (bson_iter_init_find (&iter, doc, "versionArray") &&
          bson_iter_recurse (&iter, &child)) {
         for (i = 0; i < 3; i++) {
            if (bson_iter_next (&child) && BSON_ITER_HOLDS_INT32 (&child)) {
               version = (version << 8) | (bson_iter_int32 (&child) & 0xFF);
            }
         }
      }
   }

   mongoc_database_destroy (database);
   mongoc_cursor_destroy (cursor);
   bson_destroy (&cmd);

   return version;
}


static bson_bool_t
version_check (mongoc_client_t *client,
               int              a,
               int              b,
               int              c)
{
   int version;

   version = get_version_hex (client);
   return version >= MONGOD_VERSION_HEX (a, b, c);
}


static void
test_mongoc_client_authenticate (void)
{
   mongoc_collection_t *collection;
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q;

   client = mongoc_client_new (gTestUri);
   if (version_check (client, 2, 5, 0)) {
      MONGOC_DEBUG ("Skipping test, 2.5.x not yet implemented.");
      TEST_RESULT = "SKIP";
      mongoc_client_destroy (client);
      return;
   }
   mongoc_client_destroy (client);

   /*
    * Add a user to the test database.
    */
   client = mongoc_client_new(gTestUri);
   database = mongoc_client_get_database(client, "test");
   r = mongoc_database_add_user(database, "testuser", "testpass", NULL, NULL, &error);
   assert_cmpint(r, ==, 1);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   client = mongoc_client_new(gTestUriWithPassword);
   collection = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r) {
      r = mongoc_cursor_error(cursor, &error);
      if (r) MONGOC_ERROR("%s", error.message);
      assert(!r);
   }
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}


static void
test_mongoc_client_authenticate_failure (void)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q;

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   client = mongoc_client_new(gTestUriWithBadPassword);
   collection = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   assert(!r);
   r = mongoc_cursor_error(cursor, &error);
   assert(r);
   assert(error.domain == MONGOC_ERROR_CLIENT);
   assert(error.code == MONGOC_ERROR_CLIENT_AUTHENTICATE);
   mongoc_cursor_destroy(cursor);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}


static void
test_wire_version (void)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   mock_server_t *server;
   bson_uint16_t port;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_t q = BSON_INITIALIZER;
   char *uristr;

   port = 20000 + (rand () % 1000);

   server = mock_server_new ("127.0.0.1", port, NULL, NULL);
   mock_server_set_wire_version (server, 10, 11);
   mock_server_run_in_thread (server);

   usleep (5000);

   uristr = bson_strdup_printf ("mongodb://127.0.0.1:%hu/", port);
   client = mongoc_client_new (uristr);

   collection = mongoc_client_get_collection (client, "test", "test");

   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    1,
                                    &q,
                                    NULL,
                                    NULL);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);

   r = mongoc_cursor_error (cursor, &error);
   assert (r);

   assert (error.domain == MONGOC_ERROR_PROTOCOL);
   assert (error.code == MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION);

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mock_server_quit (server, 0);
   mongoc_client_destroy (client);
   bson_free (uristr);
}


static void
read_prefs_handler (mock_server_t   *server,
                    mongoc_stream_t *stream,
                    mongoc_rpc_t    *rpc,
                    void            *user_data)
{
   bson_bool_t *success = user_data;
   bson_int32_t len;
   bson_iter_t iter;
   bson_iter_t child;
   bson_iter_t child2;
   bson_iter_t child3;
   bson_t b;
   bson_t reply = BSON_INITIALIZER;
   int r;

   if (rpc->header.opcode == MONGOC_OPCODE_QUERY) {
      memcpy (&len, rpc->query.query, 4);
      len = BSON_UINT32_FROM_LE (len);

      r = bson_init_static (&b, rpc->query.query, len);
      assert (r);

      r = bson_iter_init_find (&iter, &b, "$query");
      assert (r);
      assert (BSON_ITER_HOLDS_DOCUMENT (&iter));

      r = bson_iter_init_find (&iter, &b, "$readPreference");
      assert (r);
      assert (BSON_ITER_HOLDS_DOCUMENT (&iter));

      r = bson_iter_recurse (&iter, &child);
      assert (r);

      r = bson_iter_next (&child);
      assert (r);
      assert (BSON_ITER_HOLDS_UTF8 (&child));
      assert (!strcmp ("mode", bson_iter_key (&child)));
      assert (!strcmp ("secondaryPreferred", bson_iter_utf8 (&child, NULL)));

      r = bson_iter_next (&child);
      assert (r);
      assert (BSON_ITER_HOLDS_ARRAY (&child));

      r = bson_iter_recurse (&child, &child2);
      assert (r);

      r = bson_iter_next (&child2);
      assert (r);
      assert (BSON_ITER_HOLDS_DOCUMENT (&child2));

      r = bson_iter_recurse (&child2, &child3);
      assert (r);

      r = bson_iter_next (&child3);
      assert (r);
      assert (BSON_ITER_HOLDS_UTF8 (&child3));
      assert (!strcmp ("dc", bson_iter_key (&child3)));
      assert (!strcmp ("ny", bson_iter_utf8 (&child3, NULL)));
      r = bson_iter_next (&child3);
      assert (!r);

      r = bson_iter_next (&child2);
      assert (r);

      r = bson_iter_recurse (&child2, &child3);
      assert (r);

      r = bson_iter_next (&child3);
      assert (!r);

      mock_server_reply_simple (server, stream, rpc, MONGOC_REPLY_NONE, &reply);

      *success = TRUE;
   }
}


static void
test_mongoc_client_read_prefs (void)
{
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   mock_server_t *server;
   bson_uint16_t port;
   const bson_t *doc;
   bson_error_t error;
   bson_bool_t r;
   bson_bool_t success = FALSE;
   bson_t b = BSON_INITIALIZER;
   bson_t q = BSON_INITIALIZER;
   char *uristr;

   port = 20000 + (rand () % 1000);

   server = mock_server_new ("127.0.0.1", port, read_prefs_handler, &success);
   mock_server_run_in_thread (server);

   usleep (5000);

   uristr = bson_strdup_printf ("mongodb://127.0.0.1:%hu/", port);
   client = mongoc_client_new (uristr);

   if (!_mongoc_client_warm_up (client, &error)) {
      assert (FALSE);
   }

   collection = mongoc_client_get_collection (client, "test", "test");

   bson_append_utf8 (&b, "dc", 2, "ny", 2);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);
   mongoc_read_prefs_add_tag (read_prefs, &b);
   mongoc_read_prefs_add_tag (read_prefs, NULL);
   mongoc_collection_set_read_prefs (collection, read_prefs);

   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    1,
                                    &q,
                                    NULL,
                                    read_prefs);

   r = mongoc_cursor_next (cursor, &doc);

   usleep (50000);

   assert (success);

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mock_server_quit (server, 0);
   bson_destroy (&b);
   bson_free (uristr);
}


static void
test_mongoc_client_command (void)
{
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_bool_t r;
   bson_t cmd = BSON_INITIALIZER;

   client = mongoc_client_new (gTestUri);
   assert (client);

   bson_append_int32 (&cmd, "ping", 4, 1);

   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_NONE, 0, 1, &cmd, NULL, NULL);

   r = mongoc_cursor_next (cursor, &doc);
   assert (r);
   assert (doc);

   r = mongoc_cursor_next (cursor, &doc);
   assert (!r);
   assert (!doc);

   mongoc_cursor_destroy (cursor);
   mongoc_client_destroy (client);
   bson_destroy (&cmd);
}


static void
log_handler (mongoc_log_level_t  log_level,
             const char         *domain,
             const char         *message,
             void               *user_data)
{
   /* Do Nothing */
}


static void
seed_rand (void)
{
   int seed;
   int fd;
   int n_read;

   fd = open ("/dev/urandom", O_RDONLY);
   assert (fd != -1);

   n_read = read (fd, &seed, 4);
   assert (n_read == 4);

   fprintf (stderr, "srand(%u)\n", seed);
   srand (seed);
}


int
main (int   argc,
      char *argv[])
{
   if (argc <= 1 || !!strcmp(argv[1], "-v")) {
      mongoc_log_set_handler(log_handler, NULL);
   }

   gTestUri = bson_strdup_printf("mongodb://%s:27017/", HOST);
   gTestUriWithPassword = bson_strdup_printf("mongodb://testuser:testpass@%s:27017/test", HOST);
   gTestUriWithBadPassword = bson_strdup_printf("mongodb://baduser:badpass@%s:27017/test", HOST);

   seed_rand ();

   run_test("/mongoc/client/wire_version", test_wire_version);
   run_test("/mongoc/client/authenticate", test_mongoc_client_authenticate);
   run_test("/mongoc/client/authenticate_failure", test_mongoc_client_authenticate_failure);
   run_test("/mongoc/client/read_prefs", test_mongoc_client_read_prefs);
   run_test("/mongoc/client/command", test_mongoc_client_command);

   bson_free(gTestUri);
   bson_free(gTestUriWithPassword);
   bson_free(gTestUriWithBadPassword);

   return 0;
}
