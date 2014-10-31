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
   mongoc_bulk_operation_remove (bulk, &del);
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


static void
test_index_offset (void)
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

   collection = get_test_collection (client, "test_index_offset");
   assert (collection);

   doc = bson_new ();
   BSON_APPEND_INT32 (doc, "abcd", 1234);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error);
   assert (r);
   bson_destroy (doc);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   sel = BCON_NEW ("abcd", BCON_INT32 (1234));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_remove_one (bulk, sel);
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
   assert (bson_iter_int32 (&iter) == 1);

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
   assert (bson_iter_int32 (&citer) == 1);
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

static void
test_bulk_edge_over_1000 (void)
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_bulk_operation_t * bulk_op;
   mongoc_write_concern_t * wc = mongoc_write_concern_new();
   bson_iter_t iter, error_iter, index;
   bson_t doc, result;
   bson_error_t error;
   int i;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "OVER_1000");
   assert (collection);

   mongoc_write_concern_set_w(wc, 1);

   bulk_op = mongoc_collection_create_bulk_operation(collection, false, wc);

   for (i = 0; i < 1010; i+=3) {
      bson_init(&doc);
      bson_append_int32(&doc, "_id", -1, i);

      mongoc_bulk_operation_insert(bulk_op, &doc);

      bson_destroy(&doc);
   }

   mongoc_bulk_operation_execute(bulk_op, NULL, &error);

   mongoc_bulk_operation_destroy(bulk_op);

   bulk_op = mongoc_collection_create_bulk_operation(collection, false, wc);
   for (i = 0; i < 1010; i++) {
      bson_init(&doc);
      bson_append_int32(&doc, "_id", -1, i);

      mongoc_bulk_operation_insert(bulk_op, &doc);

      bson_destroy(&doc);
   }

   mongoc_bulk_operation_execute(bulk_op, &result, &error);

   bson_iter_init_find(&iter, &result, "writeErrors");
   assert(bson_iter_recurse(&iter, &error_iter));
   assert(bson_iter_next(&error_iter));

   for (i = 0; i < 1010; i+=3) {
      assert(bson_iter_recurse(&error_iter, &index));
      assert(bson_iter_find(&index, "index"));
      if (bson_iter_int32(&index) != i) {
          fprintf(stderr, "index should be %d, but is %d\n", i, bson_iter_int32(&index));
      }
      assert(bson_iter_int32(&index) == i);
      bson_iter_next(&error_iter);
   }

   mongoc_bulk_operation_destroy(bulk_op);
   bson_destroy (&result);

   mongoc_write_concern_destroy(wc);

   mongoc_collection_destroy(collection);
   mongoc_client_destroy(client);
}

static void
test_bulk_edge_case_372 (void)
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_bulk_operation_t *bulk;
   bson_error_t error;
   bson_iter_t iter;
   bson_iter_t citer;
   bson_iter_t child;
   const char *str;
   bson_t *selector;
   bson_t *update;
   bson_t reply;
   bool r;
   int vmaj = 0;
   int vmin = 0;
   int vmic = 0;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "CDRIVER_372");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   selector = BCON_NEW ("_id", BCON_INT32 (0));
   update = BCON_NEW ("$set", "{", "a", BCON_INT32 (0), "}");
   mongoc_bulk_operation_update_one (bulk, selector, update, true);
   bson_destroy (selector);
   bson_destroy (update);

   selector = BCON_NEW ("a", BCON_INT32 (1));
   update = BCON_NEW ("_id", BCON_INT32 (1));
   mongoc_bulk_operation_replace_one (bulk, selector, update, true);
   bson_destroy (selector);
   bson_destroy (update);

   r = mongoc_client_get_server_status (client, NULL, &reply, &error);
   if (!r) fprintf (stderr, "%s\n", error.message);
   assert (r);

   if (bson_iter_init_find (&iter, &reply, "version") &&
       BSON_ITER_HOLDS_UTF8 (&iter) &&
       (str = bson_iter_utf8 (&iter, NULL))) {
      sscanf (str, "%d.%d.%d", &vmaj, &vmin, &vmic);
   }

   bson_destroy (&reply);

   if (vmaj >=2 || (vmaj == 2 && vmin >= 6)) {
      /* This is just here to make the counts right in all cases. */
      selector = BCON_NEW ("_id", BCON_INT32 (2));
      update = BCON_NEW ("_id", BCON_INT32 (2));
      mongoc_bulk_operation_replace_one (bulk, selector, update, true);
      bson_destroy (selector);
      bson_destroy (update);
   } else {
      /* This case is only possible in MongoDB versions before 2.6. */
      selector = BCON_NEW ("_id", BCON_INT32 (3));
      update = BCON_NEW ("_id", BCON_INT32 (2));
      mongoc_bulk_operation_replace_one (bulk, selector, update, true);
      bson_destroy (selector);
      bson_destroy (update);
   }

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   if (!r) fprintf (stderr, "%s\n", error.message);
   assert (r);

#if 0
   printf ("%s\n", bson_as_json (&reply, NULL));
#endif

   assert (bson_iter_init_find (&iter, &reply, "nMatched") &&
           BSON_ITER_HOLDS_INT32 (&iter) &&
           (0 == bson_iter_int32 (&iter)));
   assert (bson_iter_init_find (&iter, &reply, "nUpserted") &&
           BSON_ITER_HOLDS_INT32 (&iter) &&
           (3 == bson_iter_int32 (&iter)));
   assert (bson_iter_init_find (&iter, &reply, "nInserted") &&
           BSON_ITER_HOLDS_INT32 (&iter) &&
           (0 == bson_iter_int32 (&iter)));
   assert (bson_iter_init_find (&iter, &reply, "nRemoved") &&
           BSON_ITER_HOLDS_INT32 (&iter) &&
           (0 == bson_iter_int32 (&iter)));

   assert (bson_iter_init_find (&iter, &reply, "upserted") &&
           BSON_ITER_HOLDS_ARRAY (&iter) &&
           bson_iter_recurse (&iter, &citer));

   assert (bson_iter_next (&citer));
   assert (BSON_ITER_HOLDS_DOCUMENT (&citer));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "_id"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (0 == bson_iter_int32 (&child));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "index"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (0 == bson_iter_int32 (&child));

   assert (bson_iter_next (&citer));
   assert (BSON_ITER_HOLDS_DOCUMENT (&citer));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "_id"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (1 == bson_iter_int32 (&child));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "index"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (1 == bson_iter_int32 (&child));

   assert (bson_iter_next (&citer));
   assert (BSON_ITER_HOLDS_DOCUMENT (&citer));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "_id"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (2 == bson_iter_int32 (&child));
   assert (bson_iter_recurse (&citer, &child));
   assert (bson_iter_find (&child, "index"));
   assert (BSON_ITER_HOLDS_INT32 (&child));
   assert (2 == bson_iter_int32 (&child));

   assert (!bson_iter_next (&citer));

   bson_destroy (&reply);

   mongoc_collection_drop (collection, NULL);

   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


static void
test_bulk_new (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t empty = BSON_INITIALIZER;
   bool r;

   client = mongoc_client_new (gTestUri);
   assert (client);

   collection = get_test_collection (client, "bulk_new");
   assert (collection);

   bulk = mongoc_bulk_operation_new (true);
   mongoc_bulk_operation_destroy (bulk);

   bulk = mongoc_bulk_operation_new (true);

   r = mongoc_bulk_operation_execute (bulk, NULL, &error);
   assert (!r);
   assert (error.domain = MONGOC_ERROR_CLIENT);
   assert (error.code = MONGOC_ERROR_COMMAND_INVALID_ARG);

   mongoc_bulk_operation_set_database (bulk, "test");
   r = mongoc_bulk_operation_execute (bulk, NULL, &error);
   assert (!r);
   assert (error.domain = MONGOC_ERROR_CLIENT);
   assert (error.code = MONGOC_ERROR_COMMAND_INVALID_ARG);

   mongoc_bulk_operation_set_collection (bulk, "test");
   r = mongoc_bulk_operation_execute (bulk, NULL, &error);
   assert (!r);
   assert (error.domain = MONGOC_ERROR_CLIENT);
   assert (error.code = MONGOC_ERROR_COMMAND_INVALID_ARG);

   mongoc_bulk_operation_set_client (bulk, client);
   r = mongoc_bulk_operation_execute (bulk, NULL, &error);
   assert (!r);
   assert (error.domain = MONGOC_ERROR_CLIENT);
   assert (error.code = MONGOC_ERROR_COMMAND_INVALID_ARG);

   mongoc_bulk_operation_insert (bulk, &empty);
   r = mongoc_bulk_operation_execute (bulk, NULL, &error);
   assert (r);

   mongoc_bulk_operation_destroy (bulk);

   mongoc_collection_drop (collection, NULL);

   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
}


void
test_bulk_install (TestSuite *suite)
{
   gTestUri = bson_strdup_printf("mongodb://%s/", MONGOC_TEST_HOST);

   TestSuite_Add (suite, "/BulkOperation/basic", test_bulk);
   TestSuite_Add (suite, "/BulkOperation/update_upserted", test_update_upserted);
   TestSuite_Add (suite, "/BulkOperation/index_offset", test_index_offset);
   TestSuite_Add (suite, "/BulkOperation/CDRIVER-372", test_bulk_edge_case_372);
   TestSuite_Add (suite, "/BulkOperation/new", test_bulk_new);
   TestSuite_Add (suite, "/BulkOperation/over_1000", test_bulk_edge_over_1000);

   atexit (cleanup_globals);
}
