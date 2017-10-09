#include "mongoc.h"
#include "mongoc-read-concern-private.h"
#include "mongoc-util-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"


static void
test_read_concern_append (void)
{
   mongoc_read_concern_t *rc;
   bson_t *cmd;

   cmd = tmp_bson ("{'foo': 1}");

   /* append valid readConcern */
   rc = mongoc_read_concern_new ();
   mongoc_read_concern_set_level (rc, MONGOC_READ_CONCERN_LEVEL_LOCAL);
   ASSERT (mongoc_read_concern_append (rc, cmd));

   ASSERT_MATCH (cmd, "{'foo': 1, 'readConcern': {'level': 'local'}}");

   mongoc_read_concern_destroy (rc);
}

static void
test_read_concern_basic (void)
{
   mongoc_read_concern_t *read_concern;

   read_concern = mongoc_read_concern_new ();

   BEGIN_IGNORE_DEPRECATIONS;

   /*
    * Test defaults.
    */
   ASSERT (read_concern);
   ASSERT (mongoc_read_concern_is_default (read_concern));
   ASSERT (!mongoc_read_concern_get_level (read_concern));

   /*
    * Test changes to level.
    */
   mongoc_read_concern_set_level (read_concern,
                                  MONGOC_READ_CONCERN_LEVEL_LOCAL);
   ASSERT (!mongoc_read_concern_is_default (read_concern));
   ASSERT_CMPSTR (mongoc_read_concern_get_level (read_concern),
                  MONGOC_READ_CONCERN_LEVEL_LOCAL);

   /*
    * Check generated bson.
    */
   ASSERT_MATCH (_mongoc_read_concern_get_bson (read_concern),
                 "{'level': 'local'}");

   mongoc_read_concern_destroy (read_concern);
}


static void
test_read_concern_bson_omits_defaults (void)
{
   mongoc_read_concern_t *read_concern;
   const bson_t *bson;
   bson_iter_t iter;

   read_concern = mongoc_read_concern_new ();

   /*
    * Check generated bson.
    */
   ASSERT (read_concern);

   bson = _mongoc_read_concern_get_bson (read_concern);
   ASSERT (!bson_iter_init_find (&iter, bson, "level"));
   ASSERT (bson);

   mongoc_read_concern_destroy (read_concern);
}


void
test_read_concern_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/ReadConcern/append", test_read_concern_append);
   TestSuite_Add (suite, "/ReadConcern/basic", test_read_concern_basic);
   TestSuite_Add (suite,
                  "/ReadConcern/bson_omits_defaults",
                  test_read_concern_bson_omits_defaults);
}
