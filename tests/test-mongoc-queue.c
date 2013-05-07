#include <mongoc.h>
#include <mongoc-queue-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_queue_basic (void)
{
   mongoc_queue_t q = MONGOC_QUEUE_INITIALIZER;

   mongoc_queue_push_head(&q, (void *)1);
   mongoc_queue_push_tail(&q, (void *)2);
   mongoc_queue_push_head(&q, (void *)3);
   mongoc_queue_push_tail(&q, (void *)4);
   mongoc_queue_push_head(&q, (void *)5);

   assert_cmpint(mongoc_queue_pop_head(&q), ==, (void *)5);
   assert_cmpint(mongoc_queue_pop_head(&q), ==, (void *)3);
   assert_cmpint(mongoc_queue_pop_head(&q), ==, (void *)1);
   assert_cmpint(mongoc_queue_pop_head(&q), ==, (void *)2);
   assert_cmpint(mongoc_queue_pop_head(&q), ==, (void *)4);
   assert(!mongoc_queue_pop_head(&q));
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/queue/basic", test_mongoc_queue_basic);

   return 0;
}
