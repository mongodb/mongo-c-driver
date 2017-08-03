#include "mongoc.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "session-test"

/* creating a session requires a crypto lib */
#ifdef MONGOC_ENABLE_SSL
static void
test_session_inheritance (void)
{
   mongoc_client_t *client;
   mongoc_session_opt_t *opts;
   mongoc_session_t *session;
   mongoc_database_t *db;
   mongoc_collection_t *collection;
   bson_error_t error;

   client = mongoc_client_new (NULL);
   opts = mongoc_session_opts_new ();

   session = mongoc_client_start_session (client, opts, &error);
   ASSERT_OR_PRINT (session, error);

   db = mongoc_session_get_database (session, "db");
   BSON_ASSERT (session == mongoc_database_get_session (db));

   /* test the two functions for getting a collection from a session */
   collection = mongoc_database_get_collection (db, "collection");
   BSON_ASSERT (session == mongoc_collection_get_session (collection));
   mongoc_collection_destroy (collection);

   collection = mongoc_session_get_collection (session, "db", "collection");
   BSON_ASSERT (session == mongoc_collection_get_session (collection));

   mongoc_collection_destroy (collection);
   mongoc_database_destroy (db);
   mongoc_session_destroy (session);
   mongoc_session_opts_destroy (opts);
   mongoc_client_destroy (client);
}
#endif

static void
test_session_opts_clone (void)
{
   mongoc_session_opt_t *opts;
   mongoc_session_opt_t *clone;

   opts = mongoc_session_opts_new ();
   mongoc_session_opts_set_causally_consistent_reads (opts, true);
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (mongoc_session_opts_get_causally_consistent_reads (clone));
   BSON_ASSERT (!mongoc_session_opts_get_retry_writes (clone));
   mongoc_session_opts_set_causally_consistent_reads (clone, false);

   mongoc_session_opts_destroy (opts);
   mongoc_session_opts_destroy (clone);
}


/* test logical session id */
typedef struct {
   mongoc_session_t *session;
   int n_cmds;
} lsid_test_t;


static void
test_session_lsid_cmd_started_cb (const mongoc_apm_command_started_t *event)
{
   lsid_test_t *test;
   const bson_t *cmd;
   const bson_t *session_lsid;
   bson_iter_t iter;
   bson_t lsid;

   test = (lsid_test_t *) mongoc_apm_command_started_get_context (event);
   session_lsid = mongoc_session_get_session_id (test->session);

   cmd = mongoc_apm_command_started_get_command (event);
   BSON_ASSERT (bson_iter_init_find (&iter, cmd, "lsid"));
   BSON_ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
   bson_iter_bson (&iter, &lsid);
   BSON_ASSERT (bson_equal (session_lsid, &lsid));

   test->n_cmds++;
}


static void
test_session_lsid_read (void *ctx)
{
   lsid_test_t test;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_session_t *session;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bool r;

   test.n_cmds = 0;

   client = test_framework_client_new ();
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks,
                                      test_session_lsid_cmd_started_cb);
   mongoc_client_set_apm_callbacks (client, callbacks, &test /* context */);
   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);
   test.session = session;

   /*
    * generic command, aggregate, and find must all have lsid
    *
    * test generic command first
    */
   r = mongoc_session_read_command_with_opts (
      session, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, NULL, &error);

   ASSERT_OR_PRINT (r, error);
   ASSERT_CMPINT (test.n_cmds, ==, 1);

   /* test aggregate */
   collection = mongoc_session_get_collection (session, "db", "collection");
   cursor = mongoc_collection_aggregate (
      collection, MONGOC_QUERY_NONE, tmp_bson ("{}"), NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
   }

   ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
   ASSERT_CMPINT (test.n_cmds, ==, 2);
   mongoc_cursor_destroy (cursor);

   /* test find */
   cursor = mongoc_collection_find_with_opts (
      collection, tmp_bson ("{}"), NULL, NULL);

   while (mongoc_cursor_next (cursor, &doc)) {
   }

   ASSERT_OR_PRINT (!mongoc_cursor_error (cursor, &error), error);
   ASSERT_CMPINT (test.n_cmds, ==, 3);

   mongoc_cursor_destroy (cursor);
   mongoc_collection_destroy (collection);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_session_destroy (session);
   mongoc_client_destroy (client);
}

static void
test_session_lsid_write (void *ctx)
{
   lsid_test_t test;
   mongoc_client_t *client;
   bson_error_t error;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_session_t *session;
   mongoc_collection_t *collection;
   mongoc_bulk_operation_t *bulk;
   bool r;

   test.n_cmds = 0;

   client = test_framework_client_new ();
   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks,
                                      test_session_lsid_cmd_started_cb);
   mongoc_client_set_apm_callbacks (client, callbacks, &test /* context */);
   session = mongoc_client_start_session (client, NULL, &error);
   ASSERT_OR_PRINT (session, error);
   test.session = session;
   collection = mongoc_session_get_collection (session, "db", "collection");

   /*
    * insert and bulk must have lsid
    *
    * test insert first
    */
   r = mongoc_collection_insert (
      collection, MONGOC_INSERT_NONE, tmp_bson ("{}"), NULL, &error);

   ASSERT_OR_PRINT (r, error);
   ASSERT_CMPINT (test.n_cmds, ==, 1);

   /* test bulk */
   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   mongoc_bulk_operation_insert (bulk, tmp_bson ("{}"));
   r = (bool) mongoc_bulk_operation_execute (bulk, NULL, &error);
   ASSERT_OR_PRINT (r, error);
   ASSERT_CMPINT (test.n_cmds, ==, 2);

   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_session_destroy (session);
   mongoc_client_destroy (client);
}


void
test_session_install (TestSuite *suite)
{
#ifdef MONGOC_ENABLE_SSL
   TestSuite_Add (suite, "/Session/inheritance", test_session_inheritance);
#endif
   TestSuite_Add (suite, "/Session/opts/clone", test_session_opts_clone);
   TestSuite_AddFull (suite,
                      "/Session/lsid/read",
                      test_session_lsid_read,
                      NULL,
                      NULL,
                      test_framework_skip_if_max_wire_version_less_than_6);
   TestSuite_AddFull (suite,
                      "/Session/lsid/write",
                      test_session_lsid_write,
                      NULL,
                      NULL,
                      test_framework_skip_if_max_wire_version_less_than_6);
}
