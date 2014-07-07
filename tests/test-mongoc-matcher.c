#include "mongoc-tests.h"

#include <bcon.h>
#include <mongoc.h>
#include <mongoc-matcher-private.h>

#include "TestSuite.h"


static void
test_mongoc_matcher_basic (void)
{
   bson_t matcher_query;
   bson_t *query;
   bson_t *to_match;
   bson_t *should_fail;
   bson_error_t error;
   mongoc_matcher_t *matcher;

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

   matcher = mongoc_matcher_new (query, &error);

   assert (matcher);

   _mongoc_matcher_op_to_bson(matcher->optree, &matcher_query);

#if 0
   {
      char *out = bson_as_json(&matcher_query, NULL);
      printf("bson: %s\n", out);
      free(out);
   }
#endif

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

   bson_destroy (query);
   bson_destroy (to_match);
   bson_destroy (should_fail);
   bson_destroy (&matcher_query);

   mongoc_matcher_destroy(matcher);
}


typedef struct compare_check
{
   const char *op;
   int32_t doc;
   int32_t query;
   bool expected;
} compare_check;


static void
test_mongoc_matcher_compare (void)
{
   mongoc_matcher_t *matcher;
   compare_check checks[] = {
      { "$gt", 2, 2, false },
      { "$gte", 2, 2, true},
      { "$lt", 2, 2, false},
      { "$lte", 2, 2, true},
      { "$ne", 2, 2, false},
      { NULL }
   };
   bson_t *doc;
   bson_t *q;
   int i;

   for (i = 0; checks [i].op; i++) {
      doc = BCON_NEW ("a", BCON_INT32 (checks[i].doc));
      q = BCON_NEW ("a", "{", checks[i].op, BCON_INT32 (checks[i].query), "}");
      matcher = mongoc_matcher_new (q, NULL);
      assert (matcher);
      assert (mongoc_matcher_match (matcher, doc) == checks[i].expected);
      bson_destroy (q);
      bson_destroy (doc);
      mongoc_matcher_destroy (matcher);
   }
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
test_mongoc_matcher_eq_utf8 (void)
{
   bson_t *doc;
   bson_t *spec;
   bson_error_t error;
   mongoc_matcher_t *matcher;
   bool r;

   spec = BCON_NEW("hello", "world");
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, spec);
   BSON_ASSERT (r);
   bson_destroy (spec);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", "world");
   doc = BCON_NEW ("hello", BCON_NULL);
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (!r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", "world");
   doc = BCON_NEW ("hello", BCON_UNDEFINED);
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (!r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);
}


static void
test_mongoc_matcher_eq_int32 (void)
{
   bson_t *spec;
   bson_t *doc;
   bson_error_t error;
   mongoc_matcher_t *matcher;
   bool r;

   spec = BCON_NEW ("hello", BCON_INT32 (1234));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, spec);
   BSON_ASSERT (r);
   bson_destroy (spec);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", BCON_INT32 (1234));
   doc = BCON_NEW ("hello", BCON_INT64 (1234));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", BCON_INT32 (1234));
   doc = BCON_NEW ("hello", BCON_INT64 (4321));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (!r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);
}


static void
test_mongoc_matcher_eq_int64 (void)
{
   bson_t *spec;
   bson_t *doc;
   bson_error_t error;
   mongoc_matcher_t *matcher;
   bool r;

   spec = BCON_NEW ("hello", BCON_INT64 (1234));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, spec);
   BSON_ASSERT (r);
   bson_destroy (spec);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", BCON_INT64 (1234));
   doc = BCON_NEW ("hello", BCON_INT64 (1234));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);

   spec = BCON_NEW ("hello", BCON_INT64 (1234));
   doc = BCON_NEW ("hello", BCON_INT32 (4321));
   matcher = mongoc_matcher_new (spec, &error);
   BSON_ASSERT (matcher);
   r = mongoc_matcher_match (matcher, doc);
   BSON_ASSERT (!r);
   bson_destroy (spec);
   bson_destroy (doc);
   mongoc_matcher_destroy (matcher);
}


static void
test_mongoc_matcher_in_basic (void)
{
   mongoc_matcher_t *matcher;
   bson_error_t error;
   bool r;
   bson_t *spec;
   bson_t doc = BSON_INITIALIZER;

   spec = BCON_NEW ("key", "{",
                       "$in", "[",
                          BCON_INT32 (1),
                          BCON_INT32 (2),
                          BCON_INT32 (3),
                       "]",
                    "}");

   matcher = mongoc_matcher_new (spec, &error);
   r = mongoc_matcher_match (matcher, &doc);
   ASSERT (!r);

   bson_reinit (&doc);
   bson_append_int32 (&doc, "key", 3, 1);
   r = mongoc_matcher_match (matcher, &doc);
   ASSERT (r);

   bson_reinit (&doc);
   bson_append_int32 (&doc, "key", 3, 2);
   r = mongoc_matcher_match (matcher, &doc);
   ASSERT (r);

   bson_reinit (&doc);
   bson_append_int32 (&doc, "key", 3, 3);
   r = mongoc_matcher_match (matcher, &doc);
   ASSERT (r);

   bson_reinit (&doc);
   bson_append_int32 (&doc, "key", 3, 4);
   r = mongoc_matcher_match (matcher, &doc);
   ASSERT (!r);

   bson_destroy (&doc);
   bson_destroy (spec);
   mongoc_matcher_destroy (matcher);
}


void
test_matcher_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Matcher/basic", test_mongoc_matcher_basic);
   TestSuite_Add (suite, "/Matcher/compare", test_mongoc_matcher_compare);
   TestSuite_Add (suite, "/Matcher/bad_spec", test_mongoc_matcher_bad_spec);
   TestSuite_Add (suite, "/Matcher/eq/utf8", test_mongoc_matcher_eq_utf8);
   TestSuite_Add (suite, "/Matcher/eq/int32", test_mongoc_matcher_eq_int32);
   TestSuite_Add (suite, "/Matcher/eq/int64", test_mongoc_matcher_eq_int64);
   TestSuite_Add (suite, "/Matcher/in/basic", test_mongoc_matcher_in_basic);
}
