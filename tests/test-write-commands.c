#include <bcon.h>
#include <mongoc.h>

#include "mongoc-collection-private.h"
#include "mongoc-write-command-private.h"
#include "mongoc-write-concern.h"
#include "mongoc-write-concern-private.h"

#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mongoc-tests.h"


static mongoc_collection_t *
get_test_collection (mongoc_client_t *client,
                     const char      *prefix)
{
   mongoc_collection_t *ret;
   char *str;

   str = gen_collection_name (prefix);
   ret = mongoc_client_get_collection (client, "test", str);
   bson_free (str);

   return ret;
}


static void
test_split_insert (void)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_oid_t oid;
   bson_t **docs;
   bson_t reply = BSON_INITIALIZER;
   bson_error_t error;
   int i;

   client = test_framework_client_new ();
   assert (client);

   collection = get_test_collection (client, "test_split_insert");
   assert (collection);

   docs = (bson_t **)bson_malloc (sizeof(bson_t*) * 3000);

   for (i = 0; i < 3000; i++) {
      docs [i] = bson_new ();
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (docs [i], "_id", &oid);
   }

   _mongoc_write_result_init (&result);

   _mongoc_write_command_init_insert (&command,
                                      docs[0],
                                      true, true);

   for (i = 1; i < 3000; i++) {
      _mongoc_write_command_insert_append (&command, docs[i]);
   }

   _mongoc_write_command_execute (&command, client, 0, collection->db,
                                  collection->collection, NULL, 0, &result);

   ASSERT_OR_PRINT (_mongoc_write_result_complete (&result, &reply, &error),
                    error);
   assert (result.nInserted == 3000);

   _mongoc_write_command_destroy (&command);
   _mongoc_write_result_destroy (&result);

   ASSERT_OR_PRINT (mongoc_collection_drop (collection, &error), error);

   for (i = 0; i < 3000; i++) {
      bson_destroy (docs [i]);
   }

   bson_free (docs);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
test_invalid_write_concern (void)
{
   mongoc_write_command_t command;
   mongoc_write_result_t result;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   mongoc_write_concern_t *write_concern;
   bson_t *doc;
   bson_t reply = BSON_INITIALIZER;
   bson_error_t error;
   bool r;

   client = test_framework_client_new ();
   assert(client);

   collection = get_test_collection(client, "test_invalid_write_concern");
   assert(collection);

   write_concern = mongoc_write_concern_new();
   assert(write_concern);
   mongoc_write_concern_set_w(write_concern, 0);
   mongoc_write_concern_set_journal(write_concern, true);
   assert(!_mongoc_write_concern_is_valid(write_concern));

   doc = BCON_NEW("_id", BCON_INT32(0));

   _mongoc_write_command_init_insert(&command, doc, true, true);
   _mongoc_write_result_init (&result);

   _mongoc_write_command_execute (&command, client, 0, collection->db,
                                  collection->collection, write_concern, 0,
                                  &result);

   r = _mongoc_write_result_complete (&result, &reply, &error);

   assert(!r);
   assert(error.domain = MONGOC_ERROR_COMMAND);
   assert(error.code = MONGOC_ERROR_COMMAND_INVALID_ARG);

   _mongoc_write_command_destroy (&command);
   _mongoc_write_result_destroy (&result);

   bson_destroy(doc);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
   mongoc_write_concern_destroy(write_concern);
}

void
test_write_command_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/WriteCommand/split_insert", test_split_insert);
   TestSuite_Add (suite, "/WriteCommand/invalid_write_concern", test_invalid_write_concern);
}
