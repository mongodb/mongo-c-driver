#include "mongoc-array-private.h"
#include "mongoc-tests.h"


static void
test_array (void)
{
   mongoc_array_t *ar;
   int i;
   int v;

   ar = mongoc_array_new(sizeof i);
   assert(ar);

   for (i = 0; i < 100; i++) {
      mongoc_array_append_val(ar, i);
   }

   for (i = 0; i < 100; i++) {
      v = mongoc_array_index(ar, int, i);
      assert(v == i);
   }

   assert(ar->len == 100);
   assert(ar->allocated >= (100 * sizeof i));

   mongoc_array_destroy(ar);
}


int
main (int argc,
      char *argv[])
{
   run_test("/mongoc/array/basic", test_array);

   return 0;
}
