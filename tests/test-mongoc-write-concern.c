#include <mongoc.h>
#include <mongoc-write-concern-private.h>

#include "mongoc-tests.h"


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
   assert(write_concern);
   assert(!mongoc_write_concern_get_fsync(write_concern));
   assert(!mongoc_write_concern_get_journal(write_concern));
   assert(mongoc_write_concern_get_w(write_concern) == MONGOC_WRITE_CONCERN_W_DEFAULT);
   assert(!mongoc_write_concern_get_wtimeout(write_concern));
   assert(!mongoc_write_concern_get_wmajority(write_concern));

   mongoc_write_concern_set_fsync(write_concern, TRUE);
   assert(mongoc_write_concern_get_fsync(write_concern));
   mongoc_write_concern_set_fsync(write_concern, FALSE);
   assert(!mongoc_write_concern_get_fsync(write_concern));

   mongoc_write_concern_set_journal(write_concern, TRUE);
   assert(mongoc_write_concern_get_journal(write_concern));
   mongoc_write_concern_set_journal(write_concern, FALSE);
   assert(!mongoc_write_concern_get_journal(write_concern));

   /*
    * Test changes to w.
    */
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_MAJORITY);
   assert(mongoc_write_concern_get_wmajority(write_concern));
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_DEFAULT);
   assert(!mongoc_write_concern_get_wmajority(write_concern));
   mongoc_write_concern_set_wmajority(write_concern, 1000);
   assert(mongoc_write_concern_get_wmajority(write_concern));
   assert(mongoc_write_concern_get_wtimeout(write_concern) == 1000);
   mongoc_write_concern_set_wtimeout(write_concern, 0);
   assert(!mongoc_write_concern_get_wtimeout(write_concern));
   mongoc_write_concern_set_w(write_concern, MONGOC_WRITE_CONCERN_W_DEFAULT);
   assert(mongoc_write_concern_get_w(write_concern) == MONGOC_WRITE_CONCERN_W_DEFAULT);
   mongoc_write_concern_set_w(write_concern, 3);
   assert(mongoc_write_concern_get_w(write_concern) == 3);

   /*
    * Check generated bson.
    */
   mongoc_write_concern_set_fsync(write_concern, TRUE);
   mongoc_write_concern_set_journal(write_concern, TRUE);
   b = mongoc_write_concern_freeze(write_concern);
   assert(bson_iter_init_find(&iter, b, "fsync") && BSON_ITER_HOLDS_BOOL(&iter) && bson_iter_bool(&iter));
   assert(bson_iter_init_find(&iter, b, "j") && BSON_ITER_HOLDS_BOOL(&iter) && bson_iter_bool(&iter));
   assert(bson_iter_init_find(&iter, b, "w") && BSON_ITER_HOLDS_INT32(&iter) && bson_iter_int32(&iter) == 3);
   assert(b);

   mongoc_write_concern_destroy(write_concern);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/write_concern/basic", test_write_concern_basic);

   return 0;
}
