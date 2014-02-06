#include <bcon.h>
#include <mongoc.h>
#include <mongoc-matcher-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_matcher_basic (void)
{
   bson_t matcher_query;
   bson_t * query, *to_match, *should_fail;
   char * out;

   bson_init(&matcher_query);

   query = BCON_NEW(
      "city", "New York",
      "state", "New York",
      "favorite color", "blue",
      "name", "{", "$not", "invalid", "}",
//      "zip", "{", "$in", "[", BCON_INT32(11201), BCON_INT32(90210), "]", "}",
      "$or", "[",
         "{", "age", "{", "$lt", BCON_INT32(18), "}", "}",
         "{", "age", "{", "$gt", BCON_INT32(45), "}", "}",
      "]"
   );

   mongoc_matcher_t * matcher = mongoc_matcher_new(query);

   _mongoc_matcher_op_to_bson(matcher->optree, &matcher_query);

   out = bson_as_json(&matcher_query, NULL);

   printf("bson: %s\n", out);
   free(out);

   to_match = BCON_NEW(
      "city", "New York",
      "state", "New York",
      "favorite color", "blue",
      "zip", BCON_INT32(11201),
      "age", BCON_INT32(65)
   );

   assert(mongoc_matcher_match(matcher, to_match));

   should_fail = BCON_NEW(
      "city", "New York",
      "state", "New York",
      "favorite color", "blue",
      "zip", BCON_INT32(99999),
      "age", BCON_INT32(30)
   );

   assert(! mongoc_matcher_match(matcher, should_fail));

   bson_destroy(query);
   bson_destroy(to_match);
   bson_destroy(should_fail);

   mongoc_matcher_destroy(matcher);
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/matcher/basic", test_mongoc_matcher_basic);

   return 0;
}
