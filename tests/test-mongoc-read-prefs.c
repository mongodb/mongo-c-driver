#include <limits.h>
#include <mongoc.h>
#include <mongoc-cluster-private.h>
#include <mongoc-read-prefs-private.h>

#include "TestSuite.h"


static void
test_mongoc_read_prefs_score (void)
{
   mongoc_read_prefs_t *read_prefs;
   mongoc_cluster_node_t node = { 0 };
   bool valid;
   int score;

#define ASSERT_VALID(r) \
   valid = mongoc_read_prefs_is_valid(r); \
   ASSERT_CMPINT(valid, ==, 1)

   read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 0);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY_PREFERRED);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_SECONDARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, 1);

   node.primary = true;
   mongoc_read_prefs_set_mode(read_prefs, MONGOC_READ_PRIMARY);
   ASSERT_VALID(read_prefs);
   score = _mongoc_read_prefs_score(read_prefs, &node);
   ASSERT_CMPINT(score, ==, INT_MAX);

   mongoc_read_prefs_destroy(read_prefs);

#undef ASSERT_VALID
}


void
test_read_prefs_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/ReadPrefs/score", test_mongoc_read_prefs_score);
}
