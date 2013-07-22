#include <limits.h>
#include <mongoc.h>
#include <mongoc-cluster-private.h>
#include <mongoc-read-prefs-private.h>

#include "mongoc-tests.h"


static void
test_mongoc_read_prefs_score (void)
{
   mongoc_read_prefs_t *read_prefs;
   mongoc_cluster_node_t node = { 0 };
   bson_bool_t valid;
   int score;

#define ASSERT_VALID(r) \
   valid = mongoc_read_prefs_is_valid(r); \
   assert_cmpint(valid, ==, 1)

   read_prefs = mongoc_read_prefs_new();

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   assert_cmpint(score, ==, 0);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   assert_cmpint(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   assert_cmpint(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   assert_cmpint(score, ==, 1);

   node.primary = TRUE;
   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   assert_cmpint(score, ==, INT_MAX);

   mongoc_read_prefs_destroy(read_prefs);

#undef ASSERT_VALID
}


int
main (int   argc,
      char *argv[])
{
   run_test("/mongoc/read_prefs/score", test_mongoc_read_prefs_score);

   return 0;
}
