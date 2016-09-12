#include <mongoc.h>
#include <mongoc-util-private.h>

#include "TestSuite.h"
#include "test-conveniences.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "test-util"


static void
test_command_name (void)
{
   bson_t *commands[] = {
      tmp_bson ("{'foo': 1}"),
      tmp_bson ("{'query': {'foo': 1}}"),
      tmp_bson ("{'query': {'foo': 1}, '$readPreference': 1}"),
      tmp_bson ("{'$query': {'foo': 1}}"),
      tmp_bson ("{'$query': {'foo': 1}, '$readPreference': 1}"),
      tmp_bson ("{'$readPreference': 1, '$query': {'foo': 1}}"),
   };

   size_t i;

   for (i = 0; i < sizeof (commands) / sizeof (bson_t *); i++) {
      ASSERT_CMPSTR ("foo", _mongoc_get_command_name (commands[i]));
   }
}


void
test_util_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Util/command_name", test_command_name);
}
