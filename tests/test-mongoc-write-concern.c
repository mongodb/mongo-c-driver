#include <mongoc.h>
#include <mongoc-write-concern-private.h>

#include "TestSuite.h"


static void
test_write_concern_basic (void)
{
   mongoc_write_concern_t *write_concern;
   const bson_t *b;
   bson_iter_t iter;

   write_concern = mongoc_write_concern_new();

   /*
    * Test defaults.
    */
   ASSERT(write_concern);
   ASSERT(!mongoc_write_concern_get_fsync(write_concern));
   ASSERT(!mongoc_write_concern_get_journal(write_concern));
   ASSERT(mongoc_write_concern_get_w(write_concern) == MONGOC_WRITE_CONCERN_W_DEFAULT);
   ASSERT(!mongoc_write_concern_get_wtimeout(write_concern));
   ASSERT(!mongoc_write_concern_get_wmajority(write_concern));

   mongoc_write_concern_set_fsync(write_concern, true);
   ASSERT(mongoc_write_concern_get_fsync(write_concern));
   mongoc_write_concern_set_fsync(write_concern, false);
   ASSERT(!mongoc_write_concern_get_fsync(write_concern));

   mongoc_write_concern_set_journal(write_concern, true);
   ASSERT(mongoc_write_concern_get_journal(write_concern));
   mongoc_write_concern_set_journal(write_concern, false);
   ASSERT(!mongoc_write_concern_get_journal(write_concern));

   /*
    * Test changes to w.
    */
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_MAJORITY);
   ASSERT(mongoc_write_concern_get_wmajority(write_concern));
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_DEFAULT);
   ASSERT(!mongoc_write_concern_get_wmajority(write_concern));
   mongoc_write_concern_set_wmajority(write_concern, 1000);
   ASSERT(mongoc_write_concern_get_wmajority(write_concern));
   ASSERT(mongoc_write_concern_get_wtimeout(write_concern) == 1000);
   mongoc_write_concern_set_wtimeout(write_concern, 0);
   ASSERT(!mongoc_write_concern_get_wtimeout(write_concern));
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_DEFAULT);
   ASSERT(mongoc_write_concern_get_w(write_concern) == MONGOC_WRITE_CONCERN_W_DEFAULT);
   mongoc_write_concern_set_w(write_concern, 3);
   ASSERT(mongoc_write_concern_get_w(write_concern) == 3);

   /*
    * Check generated bson.
    */
   mongoc_write_concern_set_fsync(write_concern, true);
   mongoc_write_concern_set_journal(write_concern, true);
   b = _mongoc_write_concern_freeze(write_concern);
   ASSERT(bson_iter_init_find(&iter, b, "fsync") && BSON_ITER_HOLDS_BOOL(&iter) && bson_iter_bool(&iter));
   ASSERT(bson_iter_init_find(&iter, b, "j") && BSON_ITER_HOLDS_BOOL(&iter) && bson_iter_bool(&iter));
   ASSERT(bson_iter_init_find(&iter, b, "w") && BSON_ITER_HOLDS_INT32(&iter) && bson_iter_int32(&iter) == 3);
   ASSERT(b);

   mongoc_write_concern_destroy(write_concern);
}


void
test_write_concern_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/WriteConcern/basic", test_write_concern_basic);
}
