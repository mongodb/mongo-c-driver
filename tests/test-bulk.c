#include <bcon.h>
#include <mongoc.h>

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
cleanup_globals (void)
{
   bson_free (gTestUri);
}


static void
test_bulk (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_t reply;
   bson_t child;
   bson_t del;
   bson_t up;
   bson_t doc = BSON_INITIALIZER;
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "test_bulk");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   mongoc_bulk_operation_insert (bulk, &doc);
   mongoc_bulk_operation_insert (bulk, &doc);
   mongoc_bulk_operation_insert (bulk, &doc);
   mongoc_bulk_operation_insert (bulk, &doc);

   bson_init (&up);
   bson_append_document_begin (&up, "$set", -1, &child);
   bson_append_int32 (&child, "hello", -1, 123);
   bson_append_document_end (&up, &child);
   mongoc_bulk_operation_update (bulk, &doc, &up, false);
   bson_destroy (&up);

   bson_init (&del);
   BSON_APPEND_INT32 (&del, "hello", 123);
   mongoc_bulk_operation_delete (bulk, &del);
   bson_destroy (&del);

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &reply, "nInserted"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (bson_iter_int32 (&iter) == 4);

   /*
    * This may be omitted if we talked to a (<= 2.4.x) node, or a mongos
    * talked to a (<= 2.4.x) node.
    */
   if (bson_iter_init_find (&iter, &reply, "nModified")) {
      assert (BSON_ITER_HOLDS_INT32 (&iter));
      assert (bson_iter_int32 (&iter) == 4);
   }

   assert (bson_iter_init_find (&iter, &reply, "nRemoved"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (4 == bson_iter_int32 (&iter));

   assert (bson_iter_init_find (&iter, &reply, "nMatched"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (4 == bson_iter_int32 (&iter));

   assert (bson_iter_init_find (&iter, &reply, "nUpserted"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (!bson_iter_int32 (&iter));

   bson_destroy (&reply);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_update_upserted (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t citer;
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "test_update_upserted");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   sel = BCON_NEW ("abcd", BCON_INT32 (1234));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_update (bulk, sel, doc, true);

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   assert (bson_iter_init_find (&iter, &reply, "nUpserted"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (bson_iter_int32 (&iter) == 1);

   assert (bson_iter_init_find (&iter, &reply, "nMatched"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (bson_iter_int32 (&iter) == 0);

   assert (bson_iter_init_find (&iter, &reply, "nRemoved"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (bson_iter_int32 (&iter) == 0);

   assert (bson_iter_init_find (&iter, &reply, "nInserted"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   assert (bson_iter_int32 (&iter) == 0);

   if (bson_iter_init_find (&iter, &reply, "nModified")) {
      assert (BSON_ITER_HOLDS_INT32 (&iter));
      assert (bson_iter_int32 (&iter) == 0);
   }

   assert (bson_iter_init_find (&iter, &reply, "upserted"));
   assert (BSON_ITER_HOLDS_ARRAY (&iter));
   assert (bson_iter_recurse (&iter, &ar));
   assert (bson_iter_next (&ar));
   assert (BSON_ITER_HOLDS_DOCUMENT (&ar));
   assert (bson_iter_recurse (&ar, &citer));
   assert (bson_iter_next (&citer));
   assert (BSON_ITER_IS_KEY (&citer, "index"));
   assert (bson_iter_next (&citer));
   assert (BSON_ITER_IS_KEY (&citer, "_id"));
   assert (BSON_ITER_HOLDS_OID (&citer));
   assert (!bson_iter_next (&citer));
   assert (!bson_iter_next (&ar));

   assert (bson_iter_init_find (&iter, &reply, "writeErrors"));
   assert (BSON_ITER_HOLDS_ARRAY (&iter));
   assert (bson_iter_recurse (&iter, &ar));
   assert (!bson_iter_next (&ar));

   bson_destroy (&reply);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);

   bson_destroy (doc);
   bson_destroy (sel);
}


void
test_bulk_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/BulkOperation/basic", test_bulk);
   TestSuite_Add (suite, "/BulkOperation/update_upserted", test_update_upserted);

   atexit (cleanup_globals);
}
