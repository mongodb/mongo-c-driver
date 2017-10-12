#include "mongoc.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "session-test"


static void
test_session_opts_clone (void)
{
   mongoc_session_opt_t *opts;
   mongoc_session_opt_t *clone;

   opts = mongoc_session_opts_new ();
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (!mongoc_session_opts_get_causal_consistency (clone));
   mongoc_session_opts_destroy (clone);

   mongoc_session_opts_set_causal_consistency (opts, true);
   clone = mongoc_session_opts_clone (opts);
   BSON_ASSERT (mongoc_session_opts_get_causal_consistency (clone));
   mongoc_session_opts_destroy (clone);

   mongoc_session_opts_destroy (opts);
}


void
test_session_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Session/opts/clone", test_session_opts_clone);
}
