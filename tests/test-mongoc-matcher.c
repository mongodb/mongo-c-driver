#include <bcon.h>
#include <mongoc.h>
#include <mongoc-matcher-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_matcher_basic (void)
{
   bson_t matcher_query;
   bson_t * query;
   char * out;

   bson_init(&matcher_query);
   
   query = BCON_NEW(
      "city", "New York",
      "$or", "[",
         "{", "age", "{", "$lt", BCON_INT32(18), "}", "}",
         "{", "age", "{", "$gt", BCON_INT32(45), "}", "}",
      "]"
   );

   mongoc_matcher_t * matcher = mongoc_matcher_new(query);

   mongoc_matcher_op_to_bson(matcher->optree, &matcher_query);

   out = bson_as_json(&matcher_query, NULL);

   printf("bson: %s\n", out);
   free(out);

   mongoc_matcher_destroy(matcher);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/matcher/basic", test_mongoc_matcher_basic);

   return 0;
}
