#include <mongoc.h>

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"

int skip_if_single (void)
{
   return (test_framework_is_mongos () || test_framework_is_replset());
}

static void
server_selection_error_dns (const char *uri, const char *errmsg, bool assert_as)
{

   mongoc_client_t *client;
   mongoc_collection_t *collection;
   bson_error_t error;
   bson_t *command;
   bson_t reply;
   bool success;

   client = mongoc_client_new (uri);

   collection = mongoc_client_get_collection (client, "test", "test");

   command = tmp_bson("{'ping': 1}");
   success = mongoc_collection_command_simple (collection, command, NULL, &reply, &error);
   ASSERT_OR_PRINT(success == assert_as, error);

   if (!success && errmsg) {
      ASSERT_CMPSTR(error.message, errmsg);
   }

   bson_destroy (&reply);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}

static void
test_server_selection_error_dns_single (void)
{
   server_selection_error_dns (
      "mongodb://non-existing-localhost:27017/",
      "No suitable servers found (`serverselectiontryonce` set): [Failed to resolve 'non-existing-localhost']",
      false
   );
}

static void
test_server_selection_error_dns_multi_fail (void)
{
   server_selection_error_dns (
      "mongodb://non-existing-localhost:27017,other-non-existing-localhost:27017/",
      "No suitable servers found (`serverselectiontryonce` set): [Failed to resolve 'non-existing-localhost'] [Failed to resolve 'other-non-existing-localhost']",
      false
   );
}
static void
test_server_selection_error_dns_multi_success (void *context)
{
   server_selection_error_dns (
      "mongodb://non-existing-localhost:27017,localhost:27017,other-non-existing-localhost:27017/",
      "",
      true
   );
}

void
test_server_selection_errors_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/server_selection/errors/dns/single", test_server_selection_error_dns_single);
   TestSuite_Add (suite, "/server_selection/errors/dns/multi/fail", test_server_selection_error_dns_multi_fail);
   TestSuite_AddFull (suite, "/server_selection/errors/dns/multi/success", test_server_selection_error_dns_multi_success, NULL, NULL, skip_if_single);
}
