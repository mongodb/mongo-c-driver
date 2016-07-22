#include <fcntl.h>
#include <mongoc.h>
#include <mongoc-host-list-private.h>

#include "mongoc-client-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-util-private.h"

#ifdef MONGOC_EXPERIMENTAL_FEATURES
#include "mongoc-metadata-private.h"
#endif

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client-test"


/*
 * test_client_cmd_write_concern:
 *
 * This test ensures that there is a lack of special
 * handling for write concerns and write concern
 * errors in generic functions that support commands
 * that write.
 *
 */

static void
test_client_cmd_write_concern (void)
{
   mongoc_client_t *client;
   bson_t reply;
   bson_error_t error;
   future_t *future;
   request_t *request;
   mock_server_t *server;
   char *cmd;

   /* set up client and wire protocol version */
   server = mock_server_with_autoismaster (0);
   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));

   /* command with invalid writeConcern */
   cmd = "{'foo' : 1, "
         "'writeConcern' : {'w' : 99 }}";
   future = future_client_command_simple (client, "test",
                                          tmp_bson (cmd),
                                          NULL,
                                          &reply, &error);
   request = mock_server_receives_command (
           server, "test",
           MONGOC_QUERY_SLAVE_OK,
           cmd);
   assert (request);

   mock_server_replies_ok_and_destroys (request);
   assert (future_get_bool (future));

   future_destroy (future);

   /* standalone response */
   future = future_client_command_simple (client, "test",
                                          tmp_bson (cmd),
                                          NULL,
                                          &reply, &error);
   request = mock_server_receives_command (
           server, "test",
           MONGOC_QUERY_SLAVE_OK,
           cmd);
   assert (request);

   mock_server_replies_simple (
           request,
           "{ 'ok' : 0, 'errmsg' : 'cannot use w > 1 when a "
           "host is not replicated', 'code' : 2 }");

   assert (!future_get_bool (future));
   future_destroy (future);
   request_destroy (request);

   /* replicaset response */
   future = future_client_command_simple (client, "test",
                                          tmp_bson (cmd),
                                          NULL,
                                          &reply, &error);
   request = mock_server_receives_command (
           server, "test",
           MONGOC_QUERY_SLAVE_OK,
           cmd);
   mock_server_replies_simple (
           request,
           "{ 'ok' : 1, 'n': 1, "
           "'writeConcernError': {'code': 17, 'errmsg': 'foo'}}");
   assert (future_get_bool (future));

   future_destroy (future);
   mock_server_destroy (server);
   mongoc_client_destroy (client);
   request_destroy (request);
}


static char *
gen_test_user (void)
{
   return bson_strdup_printf ("testuser_%u_%u",
                              (unsigned)time(NULL),
                              (unsigned)gettestpid());
}


static char *
gen_good_uri (const char *username,
              const char *dbname)
{
   char *host = test_framework_get_host ();
   uint16_t port = test_framework_get_port ();
   char *uri = bson_strdup_printf ("mongodb://%s:testpass@%s:%hu/%s",
                                   username,
                                   host,
                                   port,
                                   dbname);

   bson_free (host);
   return uri;
}


static void
test_mongoc_client_authenticate (void *context)
{
   mongoc_client_t *admin_client;
   char *username;
   char *uri;
   bson_t roles;
   mongoc_database_t *database;
   char *uri_str_no_auth;
   char *uri_str_auth;
   mongoc_collection_t *collection;
   mongoc_client_t *auth_client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   bool r;
   bson_t q;

   /*
    * Log in as admin.
    */
   admin_client = test_framework_client_new ();

   /*
    * Add a user to the test database.
    */
   username = gen_test_user ();
   uri = gen_good_uri (username, "test");

   database = mongoc_client_get_database (admin_client, "test");
   mongoc_database_remove_user (database, username, &error);
   bson_init (&roles);
   BCON_APPEND (&roles,
                "0", "{", "role", "read", "db", "test", "}");

   ASSERT_OR_PRINT (mongoc_database_add_user(database, username, "testpass",
                                             &roles, NULL, &error), error);

   mongoc_database_destroy(database);

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   uri_str_no_auth = test_framework_get_uri_str_no_auth ("test");
   uri_str_auth = test_framework_add_user_password (uri_str_no_auth,
                                                    username,
                                                    "testpass");
   auth_client = mongoc_client_new (uri_str_auth);
   test_framework_set_ssl_opts (auth_client);
   collection = mongoc_client_get_collection (auth_client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r) {
      r = mongoc_cursor_error(cursor, &error);
      if (r) {
         fprintf (stderr, "Authentication failure: \"%s\"", error.message);
      }
      assert(!r);
   }

   /*
    * Remove all test users.
    */
   database = mongoc_client_get_database (admin_client, "test");
   r = mongoc_database_remove_all_users (database, &error);
   assert (r);

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   bson_free (uri_str_no_auth);
   bson_free (uri_str_auth);
   mongoc_client_destroy (auth_client);
   bson_destroy (&roles);
   bson_free (uri);
   bson_free (username);
   mongoc_database_destroy (database);
   mongoc_client_destroy (admin_client);
}


static void
test_mongoc_client_authenticate_failure (void *context)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   bool r;
   bson_t q;
   bson_t empty = BSON_INITIALIZER;
   char *host = test_framework_get_host ();
   char *uri_str_no_auth = test_framework_get_uri_str_no_auth (NULL);
   char *bad_uri_str = test_framework_add_user_password (uri_str_no_auth,
                                                         "baduser",
                                                         "badpass");

   capture_logs (true);

   /*
    * Try authenticating with bad user.
    */
   bson_init(&q);
   client = mongoc_client_new (bad_uri_str);
   test_framework_set_ssl_opts (client);

   collection = mongoc_client_get_collection(client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   assert(!r);
   r = mongoc_cursor_error(cursor, &error);
   assert(r);
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_CLIENT);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_CLIENT_AUTHENTICATE);
   mongoc_cursor_destroy(cursor);

   /*
    * Try various commands while in the failed state to ensure we get the
    * same sort of errors.
    */
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE,
                                 &empty, NULL, &error);
   assert (!r);
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_CLIENT);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_CLIENT_AUTHENTICATE);

   /*
    * Try various commands while in the failed state to ensure we get the
    * same sort of errors.
    */
   r = mongoc_collection_update (collection, MONGOC_UPDATE_NONE,
                                 &q, &empty, NULL, &error);
   assert (!r);
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_CLIENT);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_CLIENT_AUTHENTICATE);

   bson_free (host);
   bson_free (uri_str_no_auth);
   bson_free (bad_uri_str);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}


static void
test_mongoc_client_authenticate_timeout (void *context)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   bson_t reply;
   bson_error_t error;
   future_t *future;
   request_t *request;

   server = mock_server_with_autoismaster (3);
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_username (uri, "user");
   mongoc_uri_set_password (uri, "password");
   mongoc_uri_set_option_as_int32 (uri, "socketTimeoutMS", 10);
   client = mongoc_client_new_from_uri (uri);

   future = future_client_command_simple (client, "test",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL, &reply, &error);

   request = mock_server_receives_command (server, "admin",
                                           MONGOC_QUERY_SLAVE_OK,
                                           NULL);

   ASSERT (request);
   ASSERT_CMPSTR (request->command_name, "saslStart");

   /* don't reply */
   assert (!future_get_bool (future));
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_CLIENT);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_CLIENT_AUTHENTICATE);
   ASSERT_STARTSWITH (
      error.message,
      "Failed to send \"saslStart\" command with database \"admin\"");

   ASSERT_CONTAINS (error.message, "within 10 milliseconds");

   bson_destroy (&reply);
   future_destroy (future);
   request_destroy (request);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy(client);
   mock_server_destroy (server);
}


#ifdef TODO_CDRIVER_689
static void
test_wire_version (void)
{
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   mock_server_t *server;
   const bson_t *doc;
   bson_error_t error;
   bool r;
   bson_t q = BSON_INITIALIZER;

   server = mock_server_new ();
   mock_server_auto_ismaster (server, "{'ok': 1.0,"
                                       " 'ismaster': true,"
                                       " 'minWireVersion': 10,"
                                       " 'maxWireVersion': 11}");

   mock_server_run (server);

   client = mongoc_client_new_from_uri (mock_server_get_uri (server));

   collection = mongoc_client_get_collection (client, "test", "test");

   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    1,
                                    0,
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
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}
#endif


static void
test_mongoc_client_command (void)
{
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bool r;
   bson_t cmd = BSON_INITIALIZER;

   client = test_framework_client_new ();
   assert (client);

   bson_append_int32 (&cmd, "ping", 4, 1);

   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_NONE, 0, 1, 0, &cmd, NULL, NULL);

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
test_mongoc_client_command_secondary (void)
{
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   mongoc_read_prefs_t *read_prefs;
   bson_t cmd = BSON_INITIALIZER;
   const bson_t *reply;

   capture_logs (true);

   client = test_framework_client_new ();
   assert (client);

   BSON_APPEND_INT32 (&cmd, "invalid_command_here", 1);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_NONE, 0, 1, 0, &cmd, NULL, read_prefs);
   mongoc_cursor_next (cursor, &reply);

   if (test_framework_is_replset ()) {
      assert (test_framework_server_is_secondary (
         client, mongoc_cursor_get_hint (cursor)));
   }

   mongoc_read_prefs_destroy (read_prefs);

   mongoc_cursor_destroy (cursor);
   mongoc_client_destroy (client);
   bson_destroy (&cmd);
}




static void
_test_command_read_prefs (bool simple,
                          bool pooled)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_read_prefs_t *secondary_pref;
   bson_t *cmd;
   future_t *future;
   bson_error_t error;
   request_t *request;
   mongoc_cursor_t *cursor;
   const bson_t *reply;

   /* mock mongos: easiest way to test that read preference is configured */
   server = mock_mongos_new (0);
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   secondary_pref = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   mongoc_uri_set_read_prefs_t (uri, secondary_pref);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
   }

   ASSERT_CMPINT (MONGOC_READ_SECONDARY, ==, mongoc_read_prefs_get_mode (
      mongoc_client_get_read_prefs (client)));

   cmd = tmp_bson ("{'foo': 1}");

   if (simple) {
      /* simple, without read preference */
      future = future_client_command_simple (client, "db", cmd,
                                             NULL, NULL, &error);

      request = mock_server_receives_command (
         server, "db", MONGOC_QUERY_NONE, "{'foo': 1}");

      mock_server_replies_simple (request, "{'ok': 1}");
      ASSERT_OR_PRINT (future_get_bool (future), error);
      future_destroy (future);
      request_destroy (request);

      /* with read preference */
      future = future_client_command_simple (client, "db", cmd,
                                             secondary_pref, NULL, &error);

      request = mock_server_receives_command (
         server, "db", MONGOC_QUERY_SLAVE_OK,
         "{'$query': {'foo': 1},"
         " '$readPreference': {'mode': 'secondary'}}");
      mock_server_replies_simple (request, "{'ok': 1}");
      ASSERT_OR_PRINT (future_get_bool (future), error);
      future_destroy (future);
      request_destroy (request);
   } else {
      /* not simple, no read preference */
      cursor = mongoc_client_command (client, "db", MONGOC_QUERY_NONE, 0, 0, 0,
                                      cmd, NULL, NULL);
      future = future_cursor_next (cursor, &reply);
      request = mock_server_receives_command (
         server, "db", MONGOC_QUERY_NONE, "{'foo': 1}");

      mock_server_replies_simple (request, "{'ok': 1}");
      ASSERT (future_get_bool (future));
      future_destroy (future);
      request_destroy (request);
      mongoc_cursor_destroy (cursor);

      /* with read preference */
      cursor = mongoc_client_command (client, "db", MONGOC_QUERY_NONE,
                                      0, 0, 0, cmd, NULL, secondary_pref);
      future = future_cursor_next (cursor, &reply);
      request = mock_server_receives_command (
         server, "db", MONGOC_QUERY_SLAVE_OK,
         "{'$query': {'foo': 1},"
         " '$readPreference': {'mode': 'secondary'}}");

      mock_server_replies_simple (request, "{'ok': 1}");
      ASSERT (future_get_bool (future));
      future_destroy (future);
      request_destroy (request);
      mongoc_cursor_destroy (cursor);
   }

   mongoc_uri_destroy (uri);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_read_prefs_destroy (secondary_pref);
   mock_server_destroy (server);
}


static void
test_command_simple_read_prefs_single (void)
{
   _test_command_read_prefs (true, false);
}


static void
test_command_simple_read_prefs_pooled (void)
{
   _test_command_read_prefs (true, true);
}


static void
test_command_read_prefs_single (void)
{
   _test_command_read_prefs (false, false);
}


static void
test_command_read_prefs_pooled (void)
{
   _test_command_read_prefs (false, true);
}


static void
test_command_not_found (void)
{
   mongoc_client_t *client;
   const bson_t *doc;
   bson_error_t error;
   mongoc_cursor_t *cursor;

   client = test_framework_client_new ();
   cursor = mongoc_client_command (client, "test", MONGOC_QUERY_NONE,
                                   0, 0, 0,
                                   tmp_bson ("{'foo': 1}"), NULL, NULL);

   ASSERT (!mongoc_cursor_next (cursor, &doc));
   ASSERT (mongoc_cursor_error (cursor, &error));
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_QUERY);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND);

   mongoc_cursor_destroy (cursor);
   mongoc_client_destroy (client);
}


static void
test_command_not_found_simple (void)
{
   mongoc_client_t *client;
   bson_t reply;
   bson_error_t error;

   client = test_framework_client_new ();
   ASSERT (!mongoc_client_command_simple (client, "test",
                                          tmp_bson ("{'foo': 1}"),
                                          NULL, &reply, &error));

   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_QUERY);
   ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_QUERY_COMMAND_NOT_FOUND);

   bson_destroy (&reply);
   mongoc_client_destroy (client);
}


static void
test_unavailable_seeds (void)
{
   mock_server_t *servers[2];
   char **uri_strs;
   char **uri_str;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   bson_t query = BSON_INITIALIZER;
   const bson_t *doc;
   bson_error_t error;
   
   int i;

   for (i = 0; i < 2; i++) {
      servers[i] = mock_server_down ();  /* hangs up on all requests */
      mock_server_run (servers[i]);
   }
   
   uri_str = uri_strs = bson_malloc0 (7 * sizeof (char *));
   *(uri_str++) = bson_strdup_printf (
      "mongodb://%s",
      mock_server_get_host_and_port (servers[0]));

   *(uri_str++) = bson_strdup_printf (
      "mongodb://%s,%s",
      mock_server_get_host_and_port (servers[0]),
      mock_server_get_host_and_port (servers[1]));

   *(uri_str++) = bson_strdup_printf (
      "mongodb://%s,%s/?replicaSet=rs",
      mock_server_get_host_and_port (servers[0]),
      mock_server_get_host_and_port (servers[1]));

   *(uri_str++) = bson_strdup_printf (
      "mongodb://u:p@%s",
      mock_server_get_host_and_port (servers[0]));

   *(uri_str++) = bson_strdup_printf (
      "mongodb://u:p@%s,%s",
      mock_server_get_host_and_port (servers[0]),
      mock_server_get_host_and_port (servers[1]));

   *(uri_str++) = bson_strdup_printf (
      "mongodb://u:p@%s,%s/?replicaSet=rs",
      mock_server_get_host_and_port (servers[0]),
      mock_server_get_host_and_port (servers[1]));

   for (i = 0; i < (sizeof(uri_strs) / sizeof(const char *)); i++) {
      client = mongoc_client_new (uri_strs[i]);
      assert (client);

      collection = mongoc_client_get_collection (client, "test", "test");
      cursor = mongoc_collection_find (collection,
                                       MONGOC_QUERY_NONE,
                                       0,
                                       0,
                                       0,
                                       &query,
                                       NULL,
                                       NULL);

      assert (! mongoc_cursor_next (cursor, &doc));
      assert (mongoc_cursor_error (cursor, &error));
      ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_SERVER_SELECTION);
      ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_SERVER_SELECTION_FAILURE);

      mongoc_cursor_destroy (cursor);
      mongoc_collection_destroy (collection);
      mongoc_client_destroy (client);
   }

   for (i = 0; i < 2; i++) {
      mock_server_destroy (servers[i]);
   }

   bson_strfreev (uri_strs);
}


typedef enum {
   NO_CONNECT,
   CONNECT,
   RECONNECT
} connection_option_t;


static bool
responder (request_t *request,
           void *data)
{
   if (!strcmp (request->command_name, "foo")) {
      mock_server_replies_simple (request, "{'ok': 1}");
      request_destroy (request);
      return true;
   }

   return false;
}


/* mongoc_set_for_each callback */
static bool
host_equals (void *item,
             void *ctx)
{
   mongoc_server_description_t *sd;
   const char *host_and_port;

   sd = (mongoc_server_description_t *) item;
   host_and_port = (const char *) ctx;

   return !strcasecmp (sd->host.host_and_port, host_and_port);
}


/* CDRIVER-721 catch errors in _mongoc_cluster_destroy */
static void 
test_seed_list (bool rs,
                connection_option_t connection_option,
                bool pooled)
{
   mock_server_t *server;
   mock_server_t *down_servers[3];
   int i;
   char *uri_str;
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_topology_t *topology;
   mongoc_topology_description_t *td;
   mongoc_read_prefs_t *primary_pref;
   uint32_t discovered_nodes_len;
   int64_t start;
   int64_t duration_usec;
   bson_t reply;
   bson_error_t error;
   uint32_t id;

   server = mock_server_new ();
   mock_server_run (server);

   for (i = 0; i < 3; i++) {
      down_servers[i] = mock_server_down ();
      mock_server_run (down_servers[i]);
   }

   uri_str = bson_strdup_printf (
      "mongodb://%s,%s,%s,%s",
      mock_server_get_host_and_port (server),
      mock_server_get_host_and_port (down_servers[0]),
      mock_server_get_host_and_port (down_servers[1]),
      mock_server_get_host_and_port (down_servers[2]));

   uri = mongoc_uri_new (uri_str);
   assert (uri);

   if (pooled) {
      /* must be >= minHeartbeatFrequencyMS=500 or the "reconnect"
       * case won't have time to succeed */
      mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 1000);
   }

   if (rs) {
      mock_server_auto_ismaster (server,
                                 "{'ok': 1,"
                                 " 'ismaster': true,"
                                 " 'setName': 'rs',"
                                 " 'hosts': ['%s']}",
                                 mock_server_get_host_and_port (server));

      mongoc_uri_set_option_as_utf8 (uri, "replicaSet", "rs");
   } else {
      mock_server_auto_ismaster (server,
                                 "{'ok': 1,"
                                 " 'ismaster': true,"
                                 " 'msg': 'isdbgrid'}");
   }

   /* auto-respond to "foo" command */
   mock_server_autoresponds (server, responder, NULL, NULL);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
   }

   topology = client->topology;
   td = &topology->description;

   /* a mongos load-balanced connection never removes down nodes */
   discovered_nodes_len = rs ? 1 : 4;

   primary_pref = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   if (connection_option == CONNECT || connection_option == RECONNECT) {
      start = bson_get_monotonic_time ();

      /* only localhost:port responds to initial discovery. the other seeds are
       * discarded from replica set topology, but remain for sharded. */
      ASSERT_OR_PRINT (mongoc_client_command_simple (
         client, "test", tmp_bson("{'foo': 1}"),
         primary_pref, &reply, &error), error);

      /* discovery should be quick despite down servers, say < 100ms */
      duration_usec = bson_get_monotonic_time () - start;
      ASSERT_CMPTIME ((int) (duration_usec / 1000), 100);

      bson_destroy (&reply);

      ASSERT_CMPINT (discovered_nodes_len, ==, (int) td->servers->items_len);

      if (rs) {
         ASSERT_CMPINT (td->type, ==, MONGOC_TOPOLOGY_RS_WITH_PRIMARY);
      } else {
         ASSERT_CMPINT (td->type, ==, MONGOC_TOPOLOGY_SHARDED);
      }

      if (pooled) {
         /* nodes created on demand when we use servers for actual operations */
         ASSERT_CMPINT ((int) client->cluster.nodes->items_len, ==, 1);
      }
   }

   if (connection_option == RECONNECT) {
      id = mongoc_set_find_id (td->servers,
                               host_equals,
                               (void *) mock_server_get_host_and_port (server));
      ASSERT_CMPINT (id, !=, 0);
      mongoc_topology_invalidate_server (topology, id, NULL);
      if (rs) {
         ASSERT_CMPINT (td->type, ==, MONGOC_TOPOLOGY_RS_NO_PRIMARY);
      } else {
         ASSERT_CMPINT (td->type, ==, MONGOC_TOPOLOGY_SHARDED);
      }

      ASSERT_OR_PRINT (mongoc_client_command_simple (
         client, "test", tmp_bson("{'foo': 1}"),
         primary_pref, &reply, &error), error);

      /* client waited for min heartbeat to pass before reconnecting, then
       * reconnected quickly despite down servers, say < 100ms later */
      duration_usec = bson_get_monotonic_time () - start;
      ASSERT_CMPTIME ((int) (duration_usec / 1000),
                      MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS + 100);

      bson_destroy (&reply);

      ASSERT_CMPINT (discovered_nodes_len, ==, (int) td->servers->items_len);

      if (pooled) {
         ASSERT_CMPINT ((int) client->cluster.nodes->items_len, ==, 1);
      }
   }

   /* testing for crashes like CDRIVER-721 */

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_read_prefs_destroy (primary_pref);
   mongoc_uri_destroy (uri);
   bson_free (uri_str);

   for (i = 0; i < 3; i++) {
      mock_server_destroy (down_servers[i]);
   }

   mock_server_destroy (server);
}


static void
test_rs_seeds_no_connect_single (void)
{
   test_seed_list (true, NO_CONNECT, false);
}


static void
test_rs_seeds_no_connect_pooled (void)
{
   test_seed_list (true, NO_CONNECT, true);
}


static void
test_rs_seeds_connect_single (void)
{
   test_seed_list (true, CONNECT, false);
}

static void
test_rs_seeds_connect_pooled (void)
{
   test_seed_list (true, CONNECT, true);
}


static void
test_rs_seeds_reconnect_single (void)
{
   test_seed_list (true, RECONNECT, false);
}


static void
test_rs_seeds_reconnect_pooled (void)
{
   test_seed_list (true, RECONNECT, true);
}


static void
test_mongos_seeds_no_connect_single (void)
{
   test_seed_list (false, NO_CONNECT, false);
}


static void
test_mongos_seeds_no_connect_pooled (void)
{
   test_seed_list (false, NO_CONNECT, true);
}


static void
test_mongos_seeds_connect_single (void)
{
   test_seed_list (false, CONNECT, false);
}


static void
test_mongos_seeds_connect_pooled (void)
{
   test_seed_list (false, CONNECT, true);
}


static void
test_mongos_seeds_reconnect_single (void)
{
   test_seed_list (false, RECONNECT, false);
}


static void
test_mongos_seeds_reconnect_pooled (void)
{
   test_seed_list (false, RECONNECT, true);
}


static void
test_recovering (void *ctx)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_read_mode_t read_mode;
   mongoc_read_prefs_t *prefs;
   bson_error_t error;

   server = mock_server_new ();
   mock_server_run (server);

   /* server is "recovering": not master, not secondary */
   mock_server_auto_ismaster (server,
                              "{'ok': 1,"
                              " 'ismaster': false,"
                              " 'secondary': false,"
                              " 'setName': 'rs',"
                              " 'hosts': ['%s']}",
                              mock_server_get_host_and_port (server));

   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_utf8 (uri, "replicaSet", "rs");
   client = mongoc_client_new_from_uri (uri);
   prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   /* recovering member matches no read mode */
   for (read_mode = MONGOC_READ_PRIMARY;
        read_mode <= MONGOC_READ_NEAREST;
        read_mode++) {
      mongoc_read_prefs_set_mode (prefs, read_mode);
      assert (!mongoc_topology_select (client->topology,
                                       MONGOC_SS_READ,
                                       prefs, &error));
   }

   mongoc_read_prefs_destroy (prefs);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}


static void
test_server_status (void)
{
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_t reply;

   client = test_framework_client_new ();
   assert (client);

   ASSERT_OR_PRINT (mongoc_client_get_server_status (client, NULL,
                                                     &reply, &error), error);

   assert (bson_iter_init_find (&iter, &reply, "host"));
   assert (bson_iter_init_find (&iter, &reply, "version"));
   assert (bson_iter_init_find (&iter, &reply, "ok"));

   bson_destroy (&reply);

   mongoc_client_destroy (client);
}


static void
test_get_database_names (void)
{
   mock_server_t *server = mock_server_with_autoismaster (0);
   mongoc_client_t *client;
   bson_error_t error;
   future_t *future;
   request_t *request;
   char **names;

   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   future = future_client_get_database_names (client, &error);
   request = mock_server_receives_command (server,
                                            "admin",
                                            MONGOC_QUERY_SLAVE_OK,
                                            "{'listDatabases': 1}");
   mock_server_replies (
         request, 0, 0, 0, 1,
         "{'ok': 1.0, 'databases': [{'name': 'a'}, {'name': 'local'}]}");
   names = future_get_char_ptr_ptr (future);
   assert (!strcmp(names[0], "a"));
   assert (!strcmp(names[1], "local"));
   assert (NULL == names[2]);

   bson_strfreev (names);
   request_destroy (request);
   future_destroy (future);

   future = future_client_get_database_names (client, &error);
   request = mock_server_receives_command (server,
                                            "admin",
                                            MONGOC_QUERY_SLAVE_OK,
                                            "{'listDatabases': 1}");
   mock_server_replies (
         request, 0, 0, 0, 1,
         "{'ok': 0.0, 'code': 17, 'errmsg': 'err'}");

   names = future_get_char_ptr_ptr (future);
   assert (!names);
   ASSERT_CMPINT (MONGOC_ERROR_QUERY, ==, error.domain);
   ASSERT_CMPSTR ("err", error.message);

   request_destroy (request);
   future_destroy (future);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}


static void
test_mongoc_client_ipv6 (void)
{
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_t reply;

   client = mongoc_client_new ("mongodb://[::1]/");
   assert (client);

   ASSERT_OR_PRINT (mongoc_client_get_server_status (client, NULL,
                                                     &reply, &error), error);

   assert (bson_iter_init_find (&iter, &reply, "host"));
   assert (bson_iter_init_find (&iter, &reply, "version"));
   assert (bson_iter_init_find (&iter, &reply, "ok"));

   bson_destroy (&reply);

   mongoc_client_destroy (client);
}


static void
test_mongoc_client_unix_domain_socket (void *context)
{
   mongoc_client_t *client;
   bson_error_t error;
   char *uri_str;
   bson_iter_t iter;
   bson_t reply;

   uri_str = test_framework_get_unix_domain_socket_uri_str ();
   client = mongoc_client_new (uri_str);
   test_framework_set_ssl_opts (client);

   assert (client);

   ASSERT_OR_PRINT (mongoc_client_get_server_status (client, NULL,
                                                     &reply, &error), error);

   assert (bson_iter_init_find (&iter, &reply, "host"));
   assert (bson_iter_init_find (&iter, &reply, "version"));
   assert (bson_iter_init_find (&iter, &reply, "ok"));

   bson_destroy (&reply);
   mongoc_client_destroy (client);
   bson_free (uri_str);
}


static void
test_mongoc_client_mismatched_me (void)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_read_prefs_t *prefs;
   bson_error_t error;
   future_t *future;
   request_t *request;
   char *reply;

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_utf8 (uri, "replicaSet", "rs");
   client = mongoc_client_new_from_uri (uri);
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   /* any operation should fail with server selection error */
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          prefs,
                                          NULL,
                                          &error);

   request = mock_server_receives_ismaster (server);
   reply = bson_strdup_printf (
      "{'ok': 1,"
      " 'setName': 'rs',"
      " 'ismaster': false,"
      " 'secondary': true,"
      " 'me': 'foo.com',"  /* mismatched "me" field */
      " 'hosts': ['%s']}",
      mock_server_get_host_and_port (server));

   mock_server_replies_simple (request, reply);

   assert (!future_get_bool (future));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SERVER_SELECTION,
                          MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                          "No suitable servers");

   bson_free (reply);
   request_destroy (request);
   future_destroy (future);
   mongoc_read_prefs_destroy (prefs);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}


#ifdef MONGOC_ENABLE_SSL
static void
_test_mongoc_client_ssl_opts (bool pooled)
{
   char *host;
   uint16_t port;
   char *uri_str;
   char *uri_str_auth;
   char *uri_str_auth_ssl;
   mongoc_uri_t *uri;
   const mongoc_ssl_opt_t *ssl_opts;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bool ret;
   bson_error_t error;
   int add_ssl_to_uri;

   host = test_framework_get_host ();
   port = test_framework_get_port ();
   uri_str = bson_strdup_printf (
      "mongodb://%s:%d/?serverSelectionTimeoutMS=1000",
      host, port);

   uri_str_auth = test_framework_add_user_password_from_env (uri_str);
   uri_str_auth_ssl = bson_strdup_printf ("%s&ssl=true", uri_str_auth);

   ssl_opts = test_framework_get_ssl_opts ();

   /* client uses SSL once SSL options are set, regardless of "ssl=true" */
   for (add_ssl_to_uri = 0; add_ssl_to_uri < 2; add_ssl_to_uri++) {

      if (add_ssl_to_uri) {
         uri = mongoc_uri_new (uri_str_auth_ssl);
      } else {
         uri = mongoc_uri_new (uri_str_auth);
      }

      if (pooled) {
         pool = mongoc_client_pool_new (uri);
         mongoc_client_pool_set_ssl_opts (pool, ssl_opts);
         client = mongoc_client_pool_pop (pool);
      } else {
         client = mongoc_client_new_from_uri (uri);
         mongoc_client_set_ssl_opts (client, ssl_opts);
      }

      /* any operation */
      ret = mongoc_client_command_simple (client, "admin",
                                          tmp_bson ("{'ping': 1}"), NULL,
                                          NULL, &error);

      if (test_framework_get_ssl ()) {
         ASSERT_OR_PRINT (ret, error);
      } else {
         /* TODO: CDRIVER-936 check the err msg has "SSL handshake failed" */
         ASSERT (!ret);
         ASSERT_CMPINT (MONGOC_ERROR_SERVER_SELECTION, ==, error.domain);
      }

      if (pooled) {
         mongoc_client_pool_push (pool, client);
         mongoc_client_pool_destroy (pool);
      } else {
         mongoc_client_destroy (client);
      }

      mongoc_uri_destroy (uri);
   }

   bson_free (uri_str_auth_ssl);
   bson_free (uri_str_auth);
   bson_free (uri_str);
   bson_free (host);
};


static void
test_ssl_single (void)
{
   _test_mongoc_client_ssl_opts (false);
}


static void
test_ssl_pooled (void)
{
   _test_mongoc_client_ssl_opts (true);
}
#else
/* MONGOC_ENABLE_SSL is not defined */
static void
test_mongoc_client_ssl_disabled (void)
{
   capture_logs (true);
   ASSERT (NULL == mongoc_client_new ("mongodb://host/?ssl=true"));
}
#endif


static void
_test_mongoc_client_get_description (bool pooled)
{
   mongoc_client_t *client;
   mongoc_client_pool_t *pool = NULL;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   uint32_t server_id;
   mongoc_server_description_t *sd;
   mongoc_host_list_t host;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   /* bad server_id handled correctly */
   ASSERT (NULL == mongoc_client_get_server_description (client, 1234));

   collection = get_test_collection (client, "test_mongoc_client_description");
   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0,
                                    tmp_bson ("{}"), NULL, NULL);
   ASSERT (!mongoc_cursor_next (cursor, &doc));
   server_id = mongoc_cursor_get_hint (cursor);
   ASSERT (0 != server_id);
   sd = mongoc_client_get_server_description (client, server_id);
   ASSERT (sd);
   mongoc_cursor_get_host (cursor, &host);
   ASSERT (_mongoc_host_list_equal (&host,
                                    mongoc_server_description_host (sd)));

   mongoc_server_description_destroy (sd);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}


static void
test_mongoc_client_get_description_single (void)
{
   _test_mongoc_client_get_description (false);
}


static void
test_mongoc_client_get_description_pooled (void)
{
   _test_mongoc_client_get_description (true);
}


static void
test_mongoc_client_descriptions (void)
{
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;
   mongoc_server_description_t **sds;
   size_t n, expected_n;
   bson_error_t error;
   bool r;
   bson_t *ping = tmp_bson ("{'ping': 1}");
   int64_t start;

   expected_n = test_framework_server_count ();

   /*
    * single-threaded
    */
   client = test_framework_client_new ();

   /* before connecting */
   sds = mongoc_client_get_server_descriptions (client, &n);
   ASSERT_CMPSIZE_T (n, ==, (size_t) 0);
   bson_free (sds);

   /* connect */
   r = mongoc_client_command_simple (client, "db", ping, NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);
   sds = mongoc_client_get_server_descriptions (client, &n);
   ASSERT_CMPSIZE_T (n, ==, expected_n);

   mongoc_server_descriptions_destroy_all (sds, n);
   mongoc_client_destroy (client);

   /*
    * pooled
    */
   pool = test_framework_client_pool_new ();
   client = mongoc_client_pool_pop (pool);

   /* wait for background thread to discover all members */
   start = bson_get_monotonic_time ();
   do {
      _mongoc_usleep (1000);
      if (bson_get_monotonic_time() - start > 1000 * 1000) {
         MONGOC_ERROR (
            "still have %zu descriptions, not expected %zu, after 1 sec\n",
            n, expected_n);
         abort ();
      }

      sds = mongoc_client_get_server_descriptions (client, &n);
      mongoc_server_descriptions_destroy_all (sds, n);
   } while (n != expected_n);

   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
}


static void
_test_mongoc_client_select_server (bool pooled)
{
   mongoc_client_t *client;
   mongoc_client_pool_t *pool = NULL;
   mongoc_server_description_t *sd;
   const char *server_type;
   bson_error_t error;
   mongoc_read_prefs_t *prefs;

   if (pooled) {
      pool = test_framework_client_pool_new ();
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
   }

   sd = mongoc_client_select_server (client,
                                     true, /* for writes */
                                     NULL,
                                     &error);

   ASSERT (sd);
   server_type = mongoc_server_description_type (sd);
   ASSERT (!strcmp (server_type, "Standalone") ||
           !strcmp (server_type, "RSPrimary") ||
           !strcmp (server_type, "Mongos"));

   mongoc_server_description_destroy (sd);
   sd = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     NULL,
                                     &error);

   ASSERT (sd);
   server_type = mongoc_server_description_type (sd);
   ASSERT (!strcmp (server_type, "Standalone") ||
           !strcmp (server_type, "RSPrimary") ||
           !strcmp (server_type, "Mongos"));

   mongoc_server_description_destroy (sd);
   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   sd = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     prefs,
                                     &error);

   ASSERT (sd);
   server_type = mongoc_server_description_type (sd);
   ASSERT (!strcmp (server_type, "Standalone") ||
           !strcmp (server_type, "RSSecondary") ||
           !strcmp (server_type, "Mongos"));

   mongoc_server_description_destroy (sd);
   mongoc_read_prefs_destroy (prefs);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }
}


static void
test_mongoc_client_select_server_single (void)
{
   _test_mongoc_client_select_server (false);
}


static void
test_mongoc_client_select_server_pooled (void)
{
   _test_mongoc_client_select_server (true);
}


static void
_test_mongoc_client_select_server_error (bool pooled)
{
   mongoc_uri_t *uri = NULL;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_server_description_t *sd;
   bson_error_t error;
   mongoc_read_prefs_t *prefs;
   mongoc_topology_description_type_t tdtype;
   const char *server_type;

   if (pooled) {
      uri = test_framework_get_uri ();
      mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 1000);
      pool = mongoc_client_pool_new (uri);
      test_framework_set_pool_ssl_opts (pool);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new ();
      test_framework_set_ssl_opts (client);
   }

   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);
   mongoc_read_prefs_set_tags (prefs, tmp_bson ("[{'does-not-exist': 'x'}]"));
   sd = mongoc_client_select_server (client,
                                     true, /* for writes */
                                     prefs,
                                     &error);

   ASSERT (!sd);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_SERVER_SELECTION,
                          MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                          "Cannot use read preference");

   sd = mongoc_client_select_server (client,
                                     false, /* for reads */
                                     prefs,
                                     &error);

   /* Server Selection Spec: "With topology type Single, the single server is
    * always suitable for reads if it is available." */
   tdtype = client->topology->description.type;
   if (tdtype == MONGOC_TOPOLOGY_SINGLE || tdtype == MONGOC_TOPOLOGY_SHARDED) {
      ASSERT (sd);
      server_type = mongoc_server_description_type (sd);
      ASSERT (!strcmp (server_type, "Standalone") ||
              !strcmp (server_type, "Mongos"));
      mongoc_server_description_destroy (sd);
   } else {
      ASSERT (!sd);
      ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_SERVER_SELECTION);
      ASSERT_CMPINT (error.code, ==, MONGOC_ERROR_SERVER_SELECTION_FAILURE);
   }

   mongoc_read_prefs_destroy (prefs);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
      mongoc_uri_destroy (uri);
   } else {
      mongoc_client_destroy (client);
   }
}


static void
test_mongoc_client_select_server_error_single (void)
{
   _test_mongoc_client_select_server_error (false);
}


static void
test_mongoc_client_select_server_error_pooled (void)
{
   _test_mongoc_client_select_server_error (true);
}


#ifdef MONGOC_ENABLE_SSL
static bool
_cmd (mock_server_t   *server,
      mongoc_client_t *client,
      bool             server_replies,
      bson_error_t    *error)
{
   future_t *future;
   request_t *request;
   bool r;

   future = future_client_command_simple (client, "db", tmp_bson ("{'cmd': 1}"),
                                          NULL, NULL, error);
   request = mock_server_receives_command (server, "db", MONGOC_QUERY_SLAVE_OK,
                                           NULL);
   ASSERT (request);

   if (server_replies) {
      mock_server_replies_simple (request, "{'ok': 1}");
   } else {
      mock_server_hangs_up (request);
   }

   r = future_get_bool (future);

   future_destroy (future);
   request_destroy (request);

   return r;
}

static void test_client_set_ssl_copies_args (bool pooled)
{
   mongoc_uri_t *uri;
   mock_server_t *server;
   mongoc_ssl_opt_t client_opts = { 0 };
   mongoc_ssl_opt_t server_opts = { 0 };
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   char* mutable_client_ca = NULL;
   const size_t ca_bufsize = strlen (CERT_CA) + 1;

   mutable_client_ca = bson_malloc (ca_bufsize);
   bson_strncpy (mutable_client_ca, CERT_CA, ca_bufsize);

   client_opts.ca_file = mutable_client_ca;

   server_opts.weak_cert_validation = true;
   server_opts.ca_file = CERT_CA;
   server_opts.pem_file = CERT_SERVER;

   server = mock_server_with_autoismaster (0);
   mock_server_set_ssl_opts (server, &server_opts);
   mock_server_run (server);

   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 100);

   if (pooled) {
      capture_logs (true);
      pool = mongoc_client_pool_new (uri);
      mongoc_client_pool_set_ssl_opts (pool, &client_opts);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
      mongoc_client_set_ssl_opts (client, &client_opts);
   }

   /* Now change the client ca string to be something else */
   bson_strncpy (mutable_client_ca, "garbage", ca_bufsize);

   ASSERT_OR_PRINT (_cmd (server, client, true /* server replies */, &error),
                    error);


   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   bson_free (mutable_client_ca);
   mock_server_destroy (server);
   mongoc_uri_destroy (uri);
}

static void
test_ssl_client_single_copies_args (void)
{
   test_client_set_ssl_copies_args (false);
}


static void
test_ssl_client_pooled_copies_args (void)
{
   test_client_set_ssl_copies_args (true);
}


static void
_test_ssl_reconnect (bool pooled)
{
   mongoc_uri_t *uri;
   mock_server_t *server;
   mongoc_ssl_opt_t client_opts = { 0 };
   mongoc_ssl_opt_t server_opts = { 0 };
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   bson_error_t error;
   future_t *future;

   client_opts.ca_file = CERT_CA;

   server_opts.weak_cert_validation = true;
   server_opts.ca_file = CERT_CA;
   server_opts.pem_file = CERT_SERVER;

   server = mock_server_with_autoismaster (0);
   mock_server_set_ssl_opts (server, &server_opts);
   mock_server_run (server);

   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_int32 (uri, "serverSelectionTimeoutMS", 100);

   if (pooled) {
      capture_logs (true);
      pool = mongoc_client_pool_new (uri);
      mongoc_client_pool_set_ssl_opts (pool, &client_opts);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
      mongoc_client_set_ssl_opts (client, &client_opts);
   }

   ASSERT_OR_PRINT (_cmd (server, client, true /* server replies */, &error),
                    error);

   /* man-in-the-middle: certificate changed, for example expired*/
   server_opts.pem_file = CERT_EXPIRED;
   mock_server_set_ssl_opts (server, &server_opts);

   /* server closes connections */

   ASSERT (!_cmd (server, client, false /* server hangs up */, &error));
   if (pooled) {
      ASSERT_CAPTURED_LOG (
         "failed to write data because server closed the connection",
         MONGOC_LOG_LEVEL_WARNING,
         "Failure to buffer 36 bytes: Failed to buffer 36 bytes"
         " within 10000 milliseconds");
   }

   /* next operation comes on a new connection, server verification fails */
   future = future_client_command_simple (client, "db", tmp_bson ("{'cmd': 1}"),
                                          NULL, NULL, &error);

   ASSERT (!future_get_bool (future));
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_STREAM,
                          MONGOC_ERROR_STREAM_SOCKET,
                          "TLS handshake failed");

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   future_destroy (future);
   mock_server_destroy (server);
   mongoc_uri_destroy (uri);
}


static void
test_ssl_reconnect_single (void)
{
   _test_ssl_reconnect (false);
}


static void
test_ssl_reconnect_pooled (void)
{
   _test_ssl_reconnect (true);
}

#endif

#ifdef MONGOC_EXPERIMENTAL_FEATURES
static void
test_mongoc_client_application_metadata (void)
{
   enum { BUFFER_SIZE = METADATA_MAX_SIZE };
   char big_string[BUFFER_SIZE];
   const char *short_string = "hallo thar";
   mongoc_client_t *client;
   mock_server_t *server;
   mongoc_uri_t *uri;

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   client = mongoc_client_new_from_uri (uri);

   memset (big_string, 'a', BUFFER_SIZE - 1);
   big_string[BUFFER_SIZE - 1] = '\0';

   /* Check that setting too long a name causes failure */
   ASSERT (!mongoc_client_set_appname (client, big_string));

   /* Success case */
   ASSERT (mongoc_client_set_appname (client, short_string));

   /* Make sure we can't set it twice */
   ASSERT (!mongoc_client_set_appname (client, "a"));

   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}

static void
_assert_ismaster_valid (request_t *request,
                        bool       needs_meta)
{
   const bson_t *request_doc;

   ASSERT (request);
   request_doc = request_get_doc (request, 0);
   ASSERT (request_doc);
   ASSERT (bson_has_field (request_doc, "isMaster"));
   ASSERT (bson_has_field (request_doc, METADATA_FIELD) == needs_meta);
}

/* For single threaded clients, to cause an isMaster to be sent, we must wait
 * until we're overdue for a heartbeat, and then execute some command */
static future_t *
_force_ismaster_with_ping (mongoc_client_t *client,
                           int              heartbeat_ms)
{
   future_t *future;

   /* Wait until we're overdue to send an isMaster */
   _mongoc_usleep (heartbeat_ms * 2 * 1000);

   /* Send a ping */
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL,
                                          NULL,
                                          NULL);
   ASSERT (future);
   return future;
}

/* Call after we've dealt with the isMaster sent by
 * _force_ismaster_with_ping */
static void
_respond_to_ping (future_t      *future,
                  mock_server_t *server)
{
   request_t *request;

   ASSERT (future);

   request = mock_server_receives_command (server, "admin",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{'ping': 1}");

   mock_server_replies_simple (request, "{'ok': 1}");

   ASSERT (future_get_bool (future));
   future_destroy (future);
   request_destroy (request);
}

static void
_test_client_sends_metadata (bool pooled)
{
   mock_server_t *server;
   request_t *request;
   mongoc_uri_t *uri;
   future_t *future;
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;
   const char *const server_reply = "{'ok': 1, 'ismaster': true}";
   const int heartbeat_ms = 500;

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", heartbeat_ms);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);

      /* Pop a client to trigger the topology scanner */
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
      future = _force_ismaster_with_ping (client, heartbeat_ms);
   }

   request = mock_server_receives_ismaster (server);

   /* Make sure the isMaster request has a "meta" field: */
   _assert_ismaster_valid (request, true);

   mock_server_replies_simple (request, server_reply);
   request_destroy (request);

   if (!pooled) {
      _respond_to_ping (future, server);

      /* Wait until another isMaster is sent */
      future = _force_ismaster_with_ping (client, heartbeat_ms);
   }

   request = mock_server_receives_ismaster (server);
   _assert_ismaster_valid (request, false);

   mock_server_replies_simple (request, server_reply);
   request_destroy (request);

   if (!pooled) {
      _respond_to_ping (future, server);
      future = _force_ismaster_with_ping (client, heartbeat_ms);
   }

   /* Now wait for the client to send another isMaster command, but this
    * time the server hangs up */
   request = mock_server_receives_ismaster (server);
   _assert_ismaster_valid (request, false);

   mock_server_hangs_up (request);
   request_destroy (request);

   if (!pooled) {
      /* The ping wasn't sent since we hung up with isMaster */
      ASSERT (!future_get_bool (future));
      future_destroy (future);

      /* We're in cooldown for the next few seconds, so we're not
       * allowed to send isMasters. Wait for the cooldown to end. */
      _mongoc_usleep ((MONGOC_TOPOLOGY_COOLDOWN_MS + 1000) * 1000);
      future = _force_ismaster_with_ping (client, heartbeat_ms);
   }

   /* Now the client should try to reconnect. They think the server's down
    * so now they SHOULD send isMaster */
   request = mock_server_receives_ismaster (server);
   _assert_ismaster_valid (request, true);

   mock_server_replies_simple (request, server_reply);
   request_destroy (request);

   if (!pooled) {
      _respond_to_ping (future, server);
   }

   /* cleanup */
   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_uri_destroy (uri);
   mock_server_destroy (server);
}

static void
test_client_sends_metadata_single (void)
{
   _test_client_sends_metadata (false);
}

static void
test_client_sends_metadata_pooled (void)
{
   _test_client_sends_metadata (true);
}
#endif

void
test_client_install (TestSuite *suite)
{
#ifdef TODO_CDRIVER_689
   TestSuite_Add (suite, "/Client/wire_version", test_wire_version);
#endif
   if (getenv ("MONGOC_CHECK_IPV6")) {
      /* try to validate ipv6 too */
      TestSuite_Add (suite, "/Client/ipv6", test_mongoc_client_ipv6);
   }

   TestSuite_AddFull (suite, "/Client/authenticate",
                      test_mongoc_client_authenticate, NULL, NULL,
                      test_framework_skip_if_no_auth);
   TestSuite_AddFull (suite, "/Client/authenticate_failure",
                      test_mongoc_client_authenticate_failure, NULL, NULL,
                      test_framework_skip_if_no_auth);
   TestSuite_AddFull (suite, "/Client/authenticate_timeout",
                      test_mongoc_client_authenticate_timeout, NULL, NULL,
                      test_framework_skip_if_no_auth);
   TestSuite_AddLive (suite, "/Client/command", test_mongoc_client_command);
   TestSuite_AddLive (suite, "/Client/command_secondary", test_mongoc_client_command_secondary);
   TestSuite_Add (suite, "/Client/cmd_w_write_concern",
                  test_client_cmd_write_concern);
   TestSuite_Add (suite, "/Client/command/read_prefs/simple/single", test_command_simple_read_prefs_single);
   TestSuite_Add (suite, "/Client/command/read_prefs/simple/pooled", test_command_simple_read_prefs_pooled);
   TestSuite_Add (suite, "/Client/command/read_prefs/single", test_command_read_prefs_single);
   TestSuite_Add (suite, "/Client/command/read_prefs/pooled", test_command_read_prefs_pooled);
   TestSuite_AddLive (suite, "/Client/command_not_found/cursor", test_command_not_found);
   TestSuite_AddLive (suite, "/Client/command_not_found/simple", test_command_not_found_simple);
   TestSuite_Add (suite, "/Client/unavailable_seeds", test_unavailable_seeds);
   TestSuite_Add (suite, "/Client/rs_seeds_no_connect/single", test_rs_seeds_no_connect_single);
   TestSuite_Add (suite, "/Client/rs_seeds_no_connect/pooled", test_rs_seeds_no_connect_pooled);
   TestSuite_Add (suite, "/Client/rs_seeds_connect/single", test_rs_seeds_connect_single);
   TestSuite_Add (suite, "/Client/rs_seeds_connect/pooled", test_rs_seeds_connect_pooled);
   TestSuite_Add (suite, "/Client/rs_seeds_reconnect/single", test_rs_seeds_reconnect_single);
   TestSuite_Add (suite, "/Client/rs_seeds_reconnect/pooled", test_rs_seeds_reconnect_pooled);
   TestSuite_Add (suite, "/Client/mongos_seeds_no_connect/single", test_mongos_seeds_no_connect_single);
   TestSuite_Add (suite, "/Client/mongos_seeds_no_connect/pooled", test_mongos_seeds_no_connect_pooled);
   TestSuite_Add (suite, "/Client/mongos_seeds_connect/single", test_mongos_seeds_connect_single);
   TestSuite_Add (suite, "/Client/mongos_seeds_connect/pooled", test_mongos_seeds_connect_pooled);
   TestSuite_Add (suite, "/Client/mongos_seeds_reconnect/single", test_mongos_seeds_reconnect_single);
   TestSuite_Add (suite, "/Client/mongos_seeds_reconnect/pooled", test_mongos_seeds_reconnect_pooled);
   TestSuite_AddFull (suite, "/Client/recovering", test_recovering, NULL, NULL, test_framework_skip_if_slow);
   TestSuite_AddLive (suite, "/Client/server_status", test_server_status);
   TestSuite_Add (suite, "/Client/database_names", test_get_database_names);
   TestSuite_AddFull (suite, "/Client/connect/uds", test_mongoc_client_unix_domain_socket, NULL, NULL, test_framework_skip_if_no_uds);
   TestSuite_Add (suite, "/Client/mismatched_me", test_mongoc_client_mismatched_me);

#ifdef MONGOC_EXPERIMENTAL_FEATURES
   TestSuite_Add (suite, "/Client/application_metadata", test_mongoc_client_application_metadata);
   TestSuite_Add (suite, "/Client/sends_metadata_single",
                  test_client_sends_metadata_single);
   TestSuite_Add (suite, "/Client/sends_metadata_pooled",
                  test_client_sends_metadata_pooled);
#endif

#ifdef TODO_CDRIVER_689
   TestSuite_Add (suite, "/Client/wire_version", test_wire_version);
#endif

#ifdef MONGOC_ENABLE_SSL
   TestSuite_AddLive (suite, "/Client/ssl_opts/single", test_ssl_single);
   TestSuite_AddLive (suite, "/Client/ssl_opts/pooled", test_ssl_pooled);

   TestSuite_Add (suite, "/Client/ssl_opts/copies_single",
                  test_ssl_client_single_copies_args);
   TestSuite_Add (suite, "/Client/ssl_opts/copies_pooled",
                  test_ssl_client_pooled_copies_args);

#ifdef MONGOC_ENABLE_SSL_OPENSSL
   TestSuite_Add (suite, "/Client/ssl/reconnect/single",
                  test_ssl_reconnect_single);
   TestSuite_Add (suite, "/Client/ssl/reconnect/pooled",
                  test_ssl_reconnect_pooled);
#elif defined(MONGOC_ENABLE_SSL_SECURE_TRANSPORT)
#endif
#else
   TestSuite_Add (suite, "/Client/ssl_disabled", test_mongoc_client_ssl_disabled);
#endif

   TestSuite_AddLive (suite, "/Client/get_description/single", test_mongoc_client_get_description_single);
   TestSuite_AddLive (suite, "/Client/get_description/pooled", test_mongoc_client_get_description_pooled);
   TestSuite_AddLive (suite, "/Client/descriptions", test_mongoc_client_descriptions);
   TestSuite_AddLive (suite, "/Client/select_server/single", test_mongoc_client_select_server_single);
   TestSuite_AddLive (suite, "/Client/select_server/pooled", test_mongoc_client_select_server_pooled);
   TestSuite_AddLive (suite, "/Client/select_server/err/single", test_mongoc_client_select_server_error_single);
   TestSuite_AddLive (suite, "/Client/select_server/err/pooled", test_mongoc_client_select_server_error_pooled);
}
