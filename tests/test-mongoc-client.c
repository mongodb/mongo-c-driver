#include <fcntl.h>
#include <mongoc.h>

#include "mongoc-cursor-private.h"
#include "mongoc-client-private.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client-test"

static mongoc_collection_t *
get_test_collection (mongoc_client_t *client,
                     const char      *name)
{
   mongoc_collection_t *ret;
   char *str;

   str = gen_collection_name (name);
   ret = mongoc_client_get_collection (client, "test", str);
   bson_free (str);

   return ret;
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
   admin_client = test_framework_client_new (NULL);

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
   r = mongoc_database_add_user(database, username, "testpass", &roles, NULL, &error);
   ASSERT_CMPINT(r, ==, 1);
   mongoc_database_destroy(database);

   /*
    * Try authenticating with that user.
    */
   bson_init(&q);
   auth_client = test_framework_client_new (uri);
   collection = mongoc_client_get_collection (auth_client, "test", "test");
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &q, NULL, NULL);
   r = mongoc_cursor_next(cursor, &doc);
   if (!r) {
      r = mongoc_cursor_error(cursor, &error);
      if (r) {
         MONGOC_ERROR("Authentication failure: \"%s\"", error.message);
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
   mongoc_client_destroy (auth_client);
   bson_destroy (&roles);
   bson_free (uri);
   bson_free (username);
   mongoc_database_destroy (database);
   mongoc_client_destroy (admin_client);
}


int should_run_auth_tests (void)
{
#ifndef MONGOC_ENABLE_SSL
   mongoc_client_t *client = test_framework_client_new (NULL);
   uint32_t server_id = mongoc_cluster_preselect(&client->cluster, MONGOC_OPCODE_QUERY, NULL, NULL);

   if (mongoc_cluster_node_max_wire_version (&client->cluster, server_id) > 2) {
      mongoc_client_destroy (client);
      return 0;
   }
#endif

   return 1;
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
   uint16_t port = test_framework_get_port ();
   char *bad_uri_str = bson_strdup_printf (
         "mongodb://baduser:badpass@%s:%hu/test%s",
         host,
         port,
         test_framework_get_ssl () ? "?ssl=true" : "");

   /*
    * Try authenticating with bad user.
    */
   bson_init(&q);
   client = test_framework_client_new (bad_uri_str);

   collection = mongoc_client_get_collection(client, "test", "test");
   suppress_one_message ();
   cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE, 0, 1, 0,
                                   &q, NULL, NULL);
   suppress_one_message ();
   r = mongoc_cursor_next(cursor, &doc);
   assert(!r);
   r = mongoc_cursor_error(cursor, &error);
   assert(r);
   assert(error.domain == MONGOC_ERROR_CLIENT);
   assert(error.code == MONGOC_ERROR_CLIENT_AUTHENTICATE);
   mongoc_cursor_destroy(cursor);

   /*
    * Try various commands while in the failed state to ensure we get the
    * same sort of errors.
    */
   suppress_one_message ();
   suppress_one_message ();
   suppress_one_message ();
   r = mongoc_collection_insert (collection, 0, &empty, NULL, &error);
   assert (!r);
   assert (error.domain == MONGOC_ERROR_CLIENT);
   assert (error.code == MONGOC_ERROR_CLIENT_AUTHENTICATE);

   /*
    * Try various commands while in the failed state to ensure we get the
    * same sort of errors.
    */
   suppress_one_message ();
   suppress_one_message ();
   suppress_one_message ();
   r = mongoc_collection_update (collection, 0, &q, &empty, NULL, &error);
   assert (!r);
   assert (error.domain == MONGOC_ERROR_CLIENT);
   assert (error.code == MONGOC_ERROR_CLIENT_AUTHENTICATE);

   bson_free (host);
   bson_free (bad_uri_str);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
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
test_mongoc_client_read_prefs (void)
{
   mongoc_collection_t *collection;
   mongoc_read_prefs_t *read_prefs;
   mongoc_cursor_t *cursor;
   mongoc_client_t *client;
   mock_server_t *server;
   const bson_t *doc;
   bson_t b = BSON_INITIALIZER;
   bson_t q = BSON_INITIALIZER;
   future_t *future;
   request_t *request;

   server = mock_server_new ();
   mock_server_auto_ismaster (server, "{'ok': 1,"
                                       " 'ismaster': true,"
                                       " 'msg': 'isdbgrid'}");
   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));

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
                                    0,
                                    &q,
                                    NULL,
                                    read_prefs);

   future = future_cursor_next (cursor, &doc);

   request = mock_server_receives_query (
         server,
         "test.test",
         MONGOC_QUERY_NONE,
         0,
         0,
         "{'$query': {},"
         " '$readPreference': {'mode': 'secondaryPreferred',"
         "                     'tags': [{'dc': 'ny'}, {}]}}",
         NULL);

   mock_server_replies (request,
                        0,                    /* flags */
                        0,                    /* cursorId */
                        0,                    /* startingFrom */
                        1,                    /* numberReturned */
                        "{'a': 1}");

   /* mongoc_cursor_next returned true */
   assert (future_get_bool (future));

   request_destroy (request);
   future_destroy (future);
   mongoc_read_prefs_destroy (read_prefs);
   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
   bson_destroy (&b);
}


static void
test_mongoc_client_command (void)
{
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bool r;
   bson_t cmd = BSON_INITIALIZER;

   client = test_framework_client_new (NULL);
   assert (client);

   bson_append_int32 (&cmd, "ping", 4, 1);

   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_NONE, 0, 1, 0, &cmd, NULL, NULL);
   assert (!cursor->redir_primary);

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

   client = test_framework_client_new (NULL);
   assert (client);

   BSON_APPEND_INT32 (&cmd, "invalid_command_here", 1);

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY_PREFERRED);

   suppress_one_message ();
   cursor = mongoc_client_command (client, "admin", MONGOC_QUERY_NONE, 0, 1, 0, &cmd, NULL, read_prefs);

   mongoc_read_prefs_destroy (read_prefs);

   /* ensure we detected this must go to primary */
   assert (cursor->redir_primary);

   mongoc_cursor_destroy (cursor);
   mongoc_client_destroy (client);
   bson_destroy (&cmd);
}

static void
test_mongoc_client_preselect (void)
{
   mongoc_client_t *client;
   bson_error_t error;
   uint32_t node;

   client = test_framework_client_new (NULL);
   assert (client);

   node = _mongoc_client_preselect (client, MONGOC_OPCODE_INSERT,
                                    NULL, NULL, &error);
   assert (node > 0);

   mongoc_client_destroy (client);
}


static void
test_exhaust_cursor (void)
{
   mongoc_stream_t *stream;
   mongoc_write_concern_t *wr;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_cursor_t *cursor2;
   const bson_t *doc;
   bson_t q;
   bson_t b[10];
   bson_t *bptr[10];
   int i;
   bool r;
   uint32_t hint;
   bson_error_t error;
   bson_oid_t oid;

   client = test_framework_client_new (NULL);
   assert (client);

   collection = get_test_collection (client, "test_exhaust_cursor");
   assert (collection);

   mongoc_collection_drop(collection, &error);

   wr = mongoc_write_concern_new ();
   mongoc_write_concern_set_journal (wr, true);

   /* bulk insert some records to work on */
   {
      bson_init(&q);

      for (i = 0; i < 10; i++) {
         bson_init(&b[i]);
         bson_oid_init(&oid, NULL);
         bson_append_oid(&b[i], "_id", -1, &oid);
         bson_append_int32(&b[i], "n", -1, i % 2);
         bptr[i] = &b[i];
      }

      BEGIN_IGNORE_DEPRECATIONS;
      r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                         (const bson_t **)bptr, 10, wr, &error);
      END_IGNORE_DEPRECATIONS;

      if (!r) {
         MONGOC_WARNING("Insert bulk failure: %s\n", error.message);
      }
      assert(r);
   }

   /* create a couple of cursors */
   {
      cursor = mongoc_collection_find (collection, MONGOC_QUERY_EXHAUST, 0, 0, 0, &q,
                                       NULL, NULL);

      cursor2 = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                        NULL, NULL);
   }

   /* Read from the exhaust cursor, ensure that we're in exhaust where we
    * should be and ensure that an early destroy properly causes a disconnect
    * */
   {
      uint32_t local_hint;

      r = mongoc_cursor_next (cursor, &doc);
      assert (r);
      assert (doc);
      assert (cursor->in_exhaust);
      assert (client->in_exhaust);
      local_hint = cursor->hint;

      /* destroy the cursor, make sure a disconnect happened */
      mongoc_cursor_destroy (cursor);
      stream = mongoc_set_get(client->cluster.nodes, local_hint);
      assert (! stream);

      assert (! client->in_exhaust);
   }

   /* Grab a new exhaust cursor, then verify that reading from that cursor
    * (putting the client into exhaust), breaks a mid-stream read from a
    * regular cursor */
   {
      cursor = mongoc_collection_find (collection, MONGOC_QUERY_EXHAUST, 0, 0, 0, &q,
                                       NULL, NULL);

      for (i = 0; i < 5; i++) {
         r = mongoc_cursor_next (cursor2, &doc);
         assert (r);
         assert (doc);
      }

      r = mongoc_cursor_next (cursor, &doc);
      assert (r);
      assert (doc);

      doc = NULL;
      r = mongoc_cursor_next (cursor2, &doc);
      assert (!r);
      assert (!doc);

      mongoc_cursor_error(cursor2, &error);
      assert (error.domain == MONGOC_ERROR_CLIENT);
      assert (error.code == MONGOC_ERROR_CLIENT_IN_EXHAUST);

      mongoc_cursor_destroy (cursor2);
   }

   /* make sure writes fail as well */
   {
      BEGIN_IGNORE_DEPRECATIONS;
      r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                         (const bson_t **)bptr, 10, wr, &error);
      END_IGNORE_DEPRECATIONS;

      assert (!r);
      assert (error.domain == MONGOC_ERROR_CLIENT);
      assert (error.code == MONGOC_ERROR_CLIENT_IN_EXHAUST);
   }

   /* we're still in exhaust.
    *
    * 1. check that we can create a new cursor, as long as we don't read from it
    * 2. fully exhaust the exhaust cursor
    * 3. make sure that we don't disconnect at destroy
    * 4. make sure we can read the cursor we made during the exhuast
    */
   {
      cursor2 = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                        NULL, NULL);

      stream = mongoc_set_get(client->cluster.nodes, cursor->hint);
      hint = cursor->hint;

      for (i = 1; i < 10; i++) {
         r = mongoc_cursor_next (cursor, &doc);
         assert (r);
         assert (doc);
      }

      r = mongoc_cursor_next (cursor, &doc);
      assert (!r);
      assert (!doc);

      mongoc_cursor_destroy (cursor);

      assert (stream == mongoc_set_get(client->cluster.nodes, hint));

      r = mongoc_cursor_next (cursor2, &doc);
      assert (r);
      assert (doc);
   }

   bson_destroy(&q);
   for (i = 0; i < 10; i++) {
      bson_destroy(&b[i]);
   }

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_write_concern_destroy (wr);
   mongoc_cursor_destroy (cursor2);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy (client);
}


static void
test_server_status (void)
{
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_t reply;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);

   r = mongoc_client_get_server_status (client, NULL, &reply, &error);
   assert (r);

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
         "{'ok': 1.0, 'databases': [{'name': 'a'}, {'name': 'b'}]}");
   names = future_get_char_ptr_ptr (future);
   assert (!strcmp(names[0], "a"));
   assert (!strcmp(names[1], "b"));
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
   bool r;

   client = mongoc_client_new ("mongodb://[::1]/");
   assert (client);

   r = mongoc_client_get_server_status (client, NULL, &reply, &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &reply, "host"));
   assert (bson_iter_init_find (&iter, &reply, "version"));
   assert (bson_iter_init_find (&iter, &reply, "ok"));

   bson_destroy (&reply);

   mongoc_client_destroy (client);
}


void
test_client_install (TestSuite *suite)
{
   if (getenv ("MONGOC_CHECK_IPV6")) {
      /* try to validate ipv6 too */
      TestSuite_Add (suite, "/Client/ipv6", test_mongoc_client_ipv6);
   }

   TestSuite_Add (suite, "/Client/read_prefs", test_mongoc_client_read_prefs);
   TestSuite_AddFull (suite, "/Client/authenticate", test_mongoc_client_authenticate, NULL, NULL, should_run_auth_tests);
   TestSuite_AddFull (suite, "/Client/authenticate_failure", test_mongoc_client_authenticate_failure, NULL, NULL, should_run_auth_tests);
   TestSuite_Add (suite, "/Client/command", test_mongoc_client_command);
   TestSuite_Add (suite, "/Client/command_secondary", test_mongoc_client_command_secondary);
   TestSuite_Add (suite, "/Client/preselect", test_mongoc_client_preselect);
   TestSuite_Add (suite, "/Client/exhaust_cursor", test_exhaust_cursor);
   TestSuite_Add (suite, "/Client/server_status", test_server_status);
   TestSuite_Add (suite, "/Client/database_names", test_get_database_names);

#ifdef TODO_CDRIVER_689
   TestSuite_Add (suite, "/Client/wire_version", test_wire_version);
#endif
}

