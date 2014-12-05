#include <mongoc.h>

#include "mongoc-set-private.h"

#include "TestSuite.h"

static void
test_set_dtor (void * item_, void * ctx_)
{
   int *destroyed = (int *)ctx_;

   (*destroyed)++;
}

static void
test_set_new (void)
{
   void * items[10];
   int i;
   int destroyed = 0;

   mongoc_set_t * set = mongoc_set_new(2, &test_set_dtor, &destroyed);

   for (i = 0; i < 5; i++) {
      mongoc_set_add(set, i, items + i);
   }

   for (i = 0; i < 5; i++) {
      assert( mongoc_set_get(set, i) == items + i);
   }

   mongoc_set_rm(set, 0);

   assert (destroyed == 1);

   for (i = 5; i < 10; i++) {
      mongoc_set_add(set, i, items + i);
   }

   for (i = 5; i < 10; i++) {
      assert( mongoc_set_get(set, i) == items + i);
   }

   mongoc_set_rm(set, 9);
   assert (destroyed == 2);
   mongoc_set_rm(set, 5);
   assert (destroyed == 3);

   assert( mongoc_set_get(set, 1) == items + 1);
   assert( mongoc_set_get(set, 7) == items + 7);
   assert( ! mongoc_set_get(set, 5) );

   mongoc_set_add(set, 5, items + 5);
   assert( mongoc_set_get(set, 5) == items + 5);

   mongoc_set_destroy(set);
}


void
test_set_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Set/new", test_set_new);
}
