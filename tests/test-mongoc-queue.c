#include <mongoc.h>
#include <mongoc-queue-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_queue_basic (void)
{
   mongoc_queue_t q = MONGOC_QUEUE_INITIALIZER;

   _mongoc_queue_push_head(&q, (void *)1);
   _mongoc_queue_push_tail(&q, (void *)2);
   _mongoc_queue_push_head(&q, (void *)3);
   _mongoc_queue_push_tail(&q, (void *)4);
   _mongoc_queue_push_head(&q, (void *)5);

   assert_cmpint(_mongoc_queue_get_length(&q), ==, 5);

   assert(_mongoc_queue_pop_head(&q) == (void *)5);
   assert(_mongoc_queue_pop_head(&q) == (void *)3);
   assert(_mongoc_queue_pop_head(&q) == (void *)1);
   assert(_mongoc_queue_pop_head(&q) == (void *)2);
   assert(_mongoc_queue_pop_head(&q) == (void *)4);
   assert(!_mongoc_queue_pop_head(&q));
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/queue/basic", test_mongoc_queue_basic);

   return 0;
}
