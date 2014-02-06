#include <bcon.h>
#include <mongoc.h>
#include <mongoc-matcher-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_matcher_basic (void)
{
   bson_t matcher_query;
   bson_t *query;
   bson_t *to_match;
   bson_t *should_fail;
   bson_error_t error;
   char *out;

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

   mongoc_matcher_t * matcher = mongoc_matcher_new (query, &error);

   assert (matcher);

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


static void
test_mongoc_matcher_bad_spec (void)
{
   bson_t *spec;
   bson_error_t error;
   mongoc_matcher_t *matcher;

   spec = BCON_NEW("name", "{", "$abc", "invalid", "}");
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (!matcher);
   BSON_ASSERT (error.domain == MONGOC_ERROR_MATCHER);
   BSON_ASSERT (error.code == MONGOC_ERROR_MATCHER_INVALID);
   bson_destroy (spec);

   spec = BCON_NEW("name", "{", "$or", "", "}");
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (!matcher);
   BSON_ASSERT (error.domain == MONGOC_ERROR_MATCHER);
   BSON_ASSERT (error.code == MONGOC_ERROR_MATCHER_INVALID);
   bson_destroy (spec);
}


static void
test_mongoc_matcher_eq_utf8_utf8 (void)
{
   bson_t *spec;
   bson_error_t error;
   mongoc_matcher_t *matcher;
   bson_bool_t r;

   spec = BCON_NEW("hello", "world");
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, spec);
   BSON_ASSERT (r);
   bson_destroy (spec);
}


int
main (int   argc,
      char *argv[])
{
   run_test ("/mongoc/matcher/basic", test_mongoc_matcher_basic);
   run_test ("/mongoc/matcher/bad_spec", test_mongoc_matcher_bad_spec);
   run_test ("/mongoc/matcher/eq/utf8_utf8", test_mongoc_matcher_eq_utf8_utf8);

   return 0;
}
