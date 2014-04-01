#include <bcon.h>
#include <mongoc.h>

#include "mongoc-collection-private.h"
#include "mongoc-write-command-private.h"

#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mongoc-tests.h"

static char *gTestUri;


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
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "test_split_insert");
   assert (collection);

   docs = bson_malloc (sizeof(bson_t*) * 3000);

   for (i = 0; i < 3000; i++) {
      docs [i] = bson_new ();
      bson_oid_init (&oid, NULL);
      BSON_APPEND_OID (docs [i], "_id", &oid);
   }

   _mongoc_write_result_init (&result);

   _mongoc_write_command_init_insert (&command,
                                      (const bson_t * const *)docs,
                                      3000, true, true);

   _mongoc_write_command_execute (&command, client, 0, collection->db,
                                  collection->collection, NULL, &result);

   r = _mongoc_write_result_complete (&result, &reply, &error);

   assert (r);
   assert (result.nInserted == 3000);

   _mongoc_write_command_destroy (&command);
   _mongoc_write_result_destroy (&result);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   for (i = 0; i < 3000; i++) {
      bson_destroy (docs [i]);
   }

   bson_free (docs);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
cleanup_globals (void)
{
   bson_free (gTestUri);
}

void
test_write_command_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/WriteCommand/split_insert", test_split_insert);

   atexit (cleanup_globals);
}
