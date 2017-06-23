#include "mongoc.h"
#include "TestSuite.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "session-test"

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


void
test_session_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Session/inheritance", test_session_inheritance);
   TestSuite_Add (suite, "/Session/opts/clone", test_session_opts_clone);
}
