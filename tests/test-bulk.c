#include <bcon.h>
#include <mongoc.h>

#include "TestSuite.h"

#include "test-libmongoc.h"
#include "mongoc-tests.h"

/* copy str with single-quotes replaced by double. bson_free the return value.*/
char *
single_quotes_to_double (const char *str)
{
   char *result = bson_strdup (str);
   char *p;

   for (p = result; *p; p++) {
      if (*p == '\'') {
         *p = '"';
      }
   }

   return result;
}

/*--------------------------------------------------------------------------
 *
 * assert_match_json --
 *
 *       Check that a document matches an expected pattern.
 *
 *       The provided JSON is fed to mongoc_matcher_t, so it can omit
 *       fields or use $gt, $in,$and, $or, etc. For convenience,
 *       single-quotes are synonymous with double-quotes.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Aborts if doc doesn't match json_query.
 *
 *--------------------------------------------------------------------------
 */

void
assert_match_json (const bson_t *doc,
                   const char   *json_query)
{
   char *double_quoted = single_quotes_to_double (json_query);
   bson_error_t error;
   bson_t *query;
   mongoc_matcher_t *matcher;

   query = bson_new_from_json ((const uint8_t *)double_quoted, -1, &error);

   if (!query) {
      fprintf (stderr, "couldn't parse JSON: %s\n", error.message);
      abort ();
   }

   if (!(matcher = mongoc_matcher_new (query, &error))) {
      fprintf (stderr, "couldn't parse JSON: %s\n", error.message);
      abort ();
   }

   if (!mongoc_matcher_match (matcher, doc)) {
      fprintf (stderr,
               "assert_match_json failed with document:\n\n"
               "%s\n\nquery:\n\n%s\n",
               bson_as_json (doc, NULL), double_quoted);
      abort ();
   }

   mongoc_matcher_destroy (matcher);
   bson_destroy (query);
   bson_free (double_quoted);
}


/*--------------------------------------------------------------------------
 *
 * check_n_modified --
 *
 *       Check a bulk operation reply's nModified field is correct or absent.
 *
 *       It may be omitted if we talked to a (<= 2.4.x) node, or a mongos
 *       talked to a (<= 2.4.x) node.
 *
 * Returns:
 *       None.
 *
 * Side effects:
 *       Aborts if the field is present and incorrect.
 *
 *--------------------------------------------------------------------------
 */

void
check_n_modified (const bson_t *reply,
                  int32_t       n_modified)
{
   bson_iter_t iter;

   if (bson_iter_init_find (&iter, reply, "nModified")) {
      assert (BSON_ITER_HOLDS_INT32 (&iter));
      assert (bson_iter_int32 (&iter) == n_modified);
   }
}


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
test_bulk (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t reply;
   bson_t child;
   bson_t del;
   bson_t up;
   bson_t doc = BSON_INITIALIZER;
   bool r;

   client = test_framework_client_new (NULL);
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

   assert_match_json (&reply, "{'nInserted': 4,"
                              " 'nRemoved':  4,"
                              " 'nMatched':  4,"
                              " 'nUpserted': 0}");

   check_n_modified (&reply, 4);

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
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);

   collection = get_test_collection (client, "test_update_upserted");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   sel = BCON_NEW ("_id", BCON_INT32 (1234));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_update (bulk, sel, doc, true);

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   assert_match_json (&reply, "{'nInserted': 0,"
                              " 'nRemoved':  0,"
                              " 'nMatched':  0,"
                              " 'nUpserted': 1,"
                              " 'upserted': [{'index': 0, '_id': 1234}],"
                              " 'writeErrors': []}");

   check_n_modified (&reply, 0);

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
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);

   collection = get_test_collection (client, "test_index_offset");
   assert (collection);

   doc = bson_new ();
   BSON_APPEND_INT32 (doc, "_id", 1234);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error);
   assert (r);
   bson_destroy (doc);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   sel = BCON_NEW ("_id", BCON_INT32 (1234));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_remove_one (bulk, sel);
   mongoc_bulk_operation_update (bulk, sel, doc, true);

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   assert_match_json (&reply, "{'nInserted': 0,"
                              " 'nRemoved':  1,"
                              " 'nMatched':  0,"
                              " 'nUpserted': 1,"
                              " 'upserted': [{'index': 1, '_id': 1234}],"
                              " 'writeErrors': []}");

   check_n_modified (&reply, 0);

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

   client = test_framework_client_new (NULL);
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
   const char *str;
   bson_t *selector;
   bson_t *update;
   bson_t reply;
   bool r;
   int vmaj = 0;
   int vmin = 0;
   int vmic = 0;

   client = test_framework_client_new (NULL);
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

   assert_match_json (
      &reply,
      "{'nInserted': 0,"
      " 'nRemoved':  0,"
      " 'nMatched':  0,"
      " 'nUpserted': 3,"
      " 'upserted': ["
      "     {'index': 0, '_id': 0},"
      "     {'index': 1, '_id': 1},"
      "     {'index': 2, '_id': 2}"
      " ],"
      " 'writeErrors': []}");

   check_n_modified (&reply, 0);

   assert (bson_iter_init_find (&iter, &reply, "upserted") &&
           BSON_ITER_HOLDS_ARRAY (&iter) &&
           bson_iter_recurse (&iter, &citer));

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

   client = test_framework_client_new (NULL);
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
   TestSuite_Add (suite, "/BulkOperation/basic", test_bulk);
   TestSuite_Add (suite, "/BulkOperation/update_upserted", test_update_upserted);
   TestSuite_Add (suite, "/BulkOperation/index_offset", test_index_offset);
   TestSuite_Add (suite, "/BulkOperation/CDRIVER-372", test_bulk_edge_case_372);
   TestSuite_Add (suite, "/BulkOperation/new", test_bulk_new);
   TestSuite_Add (suite, "/BulkOperation/over_1000", test_bulk_edge_over_1000);
}
