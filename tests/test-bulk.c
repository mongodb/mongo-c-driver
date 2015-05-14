#include <bcon.h>
#include <mongoc.h>
#include <mongoc-bulk-operation-private.h>

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
 * match_json --
 *
 *       Check that a document matches an expected pattern.
 *
 *       The provided JSON is fed to mongoc_matcher_t, so it can omit
 *       fields or use $gt, $in, $and, $or, etc. For convenience,
 *       single-quotes are synonymous with double-quotes.
 *
 * Returns:
 *       True or false.
 *
 * Side effects:
 *       Logs if no match.
 *
 *--------------------------------------------------------------------------
 */

bool
match_json (const bson_t *doc,
            const char   *json_query,
            const char   *filename,
            int           lineno,
            const char   *funcname)
{
   char *double_quoted = single_quotes_to_double (json_query);
   bson_error_t error;
   bson_t *query;
   mongoc_matcher_t *matcher;
   bool matches;

   query = bson_new_from_json ((const uint8_t *)double_quoted, -1, &error);

   if (!query) {
      fprintf (stderr, "couldn't parse JSON: %s\n", error.message);
      abort ();
   }

   if (!(matcher = mongoc_matcher_new (query, &error))) {
      fprintf (stderr, "couldn't parse JSON: %s\n", error.message);
      abort ();
   }

   matches = mongoc_matcher_match (matcher, doc);

   if (!matches) {
      fprintf (stderr,
               "ASSERT_MATCH failed with document:\n\n"
               "%s\n"
               "query:\n%s\n\n"
               "%s:%d  %s()\n",
               bson_as_json (doc, NULL), double_quoted,
               filename, lineno, funcname);
   }

   mongoc_matcher_destroy (matcher);
   bson_destroy (query);
   bson_free (double_quoted);

   return matches;
}

#define ASSERT_MATCH(doc, json_query) \
   do { \
      assert (match_json (doc, json_query, \
                          __FILE__, __LINE__, __FUNCTION__)); \
   } while (0)


/*--------------------------------------------------------------------------
 *
 * server_has_write_commands --
 *
 *       Decide with wire version if server supports write commands
 *
 * Returns:
 *       True or false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
server_has_write_commands (mongoc_client_t *client)
{
   bson_t *ismaster_cmd = BCON_NEW ("ismaster", BCON_INT32 (1));
   bson_t ismaster;
   bson_iter_t iter;
   bool expect;

   assert (mongoc_client_command_simple (client, "admin", ismaster_cmd,
                                         NULL, &ismaster, NULL));

   expect = (bson_iter_init_find_case (&iter, &ismaster, "maxWireVersion") &&
             BSON_ITER_HOLDS_INT32 (&iter) &&
             bson_iter_int32 (&iter) > 1);

   bson_destroy (ismaster_cmd);
   bson_destroy (&ismaster);

   return expect;
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
 *       Aborts if the field is incorrect.
 *
 *--------------------------------------------------------------------------
 */

void
check_n_modified (bool          has_write_commands,
                  const bson_t *reply,
                  int32_t       n_modified)
{
   bson_iter_t iter;

   if (bson_iter_init_find (&iter, reply, "nModified")) {
      assert (has_write_commands);
      assert (BSON_ITER_HOLDS_INT32 (&iter));
      assert (bson_iter_int32 (&iter) == n_modified);
   } else {
      assert (!has_write_commands);
   }
}


/*--------------------------------------------------------------------------
 *
 * oid_created_on_client --
 *
 *       Check that a document's _id contains this process's pid.
 *
 * Returns:
 *       True or false.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

bool
oid_created_on_client (const bson_t *doc)
{
   bson_oid_t new_oid;
   const uint8_t *new_pid;
   bson_iter_t iter;
   const bson_oid_t *oid;
   const uint8_t *pid;

   bson_oid_init (&new_oid, NULL);
   new_pid = &new_oid.bytes[7];

   bson_iter_init_find (&iter, doc, "_id");

   if (!BSON_ITER_HOLDS_OID (&iter)) {
      return false;
   }

   oid = bson_iter_oid (&iter);
   pid = &oid->bytes[7];

   return 0 == memcmp (pid, new_pid, 2);
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
   bool has_write_cmds;
   bson_error_t error;
   bson_t reply;
   bson_t child;
   bson_t del;
   bson_t up;
   bson_t doc = BSON_INITIALIZER;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

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

   ASSERT_MATCH (&reply, "{'nInserted': 4,"
                         " 'nRemoved':  4,"
                         " 'nMatched':  4,"
                         " 'nUpserted': 0}");

   check_n_modified (has_write_cmds, &reply, 4);

   bson_destroy (&reply);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_insert (bool ordered)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;
   bson_error_t error;
   bson_t reply;
   bson_t doc = BSON_INITIALIZER;
   bson_t query = BSON_INITIALIZER;
   bool r;
   mongoc_cursor_t *cursor;
   const bson_t *inserted_doc;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_insert");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);
   assert (bulk->ordered == ordered);

   mongoc_bulk_operation_insert (bulk, &doc);
   mongoc_bulk_operation_insert (bulk, &doc);

   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 2,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 0}");

   check_n_modified (has_write_cmds, &reply, 0);

   bson_destroy (&reply);

   ASSERT_CMPINT (
      2, ==, (int)mongoc_collection_count (collection, MONGOC_QUERY_NONE,
                                           NULL, 0, 0, NULL, NULL));

   cursor = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0,
                                    &query, NULL, NULL);
   assert (cursor);

   while (mongoc_cursor_next (cursor, &inserted_doc)) {
      assert (oid_created_on_client (inserted_doc));
   }

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_cursor_destroy (cursor);
   bson_destroy (&query);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (&doc);
}


static void
test_insert_ordered (void)
{
   test_insert (true);
}


static void
test_insert_unordered (void)
{
   test_insert (false);
}


static void
test_insert_check_keys (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;
   bson_t *doc;
   bson_t reply;
   bson_error_t error;
   bool r;
   char *json_query;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_insert_check_keys");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   doc = BCON_NEW ("$dollar", BCON_INT32 (1));
   mongoc_bulk_operation_insert (bulk, doc);
   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (!r);
   ASSERT_CMPINT (error.domain, ==, MONGOC_ERROR_COMMAND);
   assert (error.code);
   fprintf (stderr, "TODO: CDRIVER-648, assert nInserted == 0\n");
   json_query = bson_strdup_printf ("{'nRemoved':  0,"
                                    " 'nMatched':  0,"
                                    " 'nUpserted': 0,"
                                    " 'writeErrors.0.index': 0,"
                                    " 'writeErrors.0.code': %d}",
                                    error.code);
   ASSERT_MATCH (&reply, json_query);
   check_n_modified (has_write_cmds, &reply, 0);

   bson_free (json_query);
   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (doc);
}


static void
test_upsert (bool ordered)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;

   bson_error_t error;
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_upsert");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);

   sel = BCON_NEW ("_id", BCON_INT32 (1234));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_update (bulk, sel, doc, true);

   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 1,"
                         " 'upserted':  [{'index': 0, '_id': 1234}],"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 0);

   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   bson_destroy (doc);
   bson_destroy (sel);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);

   /* non-upsert, no matches */
   sel = BCON_NEW ("_id", BCON_INT32 (2));
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");

   mongoc_bulk_operation_update (bulk, sel, doc, false);
   r = mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 0,"
                         " 'upserted':  {'$exists': false},"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 0);

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);

   bson_destroy (doc);
   bson_destroy (sel);
}


static void
test_upsert_ordered (void)
{
   test_upsert (true);
}


static void
test_upsert_unordered (void)
{
   test_upsert (false);
}


static void
test_update_one (bool ordered)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;

   bson_error_t error;
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_update_one");
   assert (collection);

   doc = bson_new ();
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc,
                                 NULL, NULL);
   assert (r);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc,
                                 NULL, NULL);
   assert (r);
   bson_destroy (doc);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);

   sel = bson_new ();
   doc = BCON_NEW ("$set", "{", "hello", "there", "}");
   mongoc_bulk_operation_update_one (bulk, sel, doc, true);
   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  1,"
                         " 'nUpserted': 0,"
                         " 'upserted': {'$exists': false},"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 1);

   bson_destroy (sel);
   sel = BCON_NEW ("hello", BCON_UTF8 ("there"));
   ASSERT_CMPINT (
      1,
      ==,
      (int)mongoc_collection_count (collection, MONGOC_QUERY_NONE, sel,
                                    0, 0, NULL, NULL));

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   bson_destroy (sel);
   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (doc);
}


static void
test_update_one_ordered ()
{
   test_update_one (true);
}


static void
test_update_one_unordered ()
{
   test_update_one (false);
}


static void
test_replace_one (bool ordered)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;

   bson_error_t error;
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_replace_one");
   assert (collection);

   doc = bson_new ();
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc,
                                 NULL, NULL);
   assert (r);
   r = mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc,
                                 NULL, NULL);
   assert (r);
   bson_destroy (doc);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);

   sel = bson_new ();
   doc = BCON_NEW ("hello", "there");
   mongoc_bulk_operation_replace_one (bulk, sel, doc, true);
   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  1,"
                         " 'nUpserted': 0,"
                         " 'upserted': {'$exists': false},"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 1);

   bson_destroy (sel);
   sel = BCON_NEW ("hello", BCON_UTF8 ("there"));
   ASSERT_CMPINT (
      1,
      ==,
      (int)mongoc_collection_count (collection, MONGOC_QUERY_NONE, sel,
                                    0, 0, NULL, NULL));

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   bson_destroy (sel);
   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   bson_destroy (doc);
}


static void
test_upsert_large ()
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;
   int huge_string_len;
   char *huge_string;
   bson_t *sel = BCON_NEW ("_id", BCON_INT32 (1));
   bson_t doc = BSON_INITIALIZER;
   bson_t child = BSON_INITIALIZER;
   bson_error_t error;
   bson_t reply;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);
   huge_string_len = (int)mongoc_client_get_max_bson_size (client) - 37;
   huge_string = bson_malloc ((size_t)huge_string_len);
   assert (huge_string);
   memset (huge_string, 'a', huge_string_len - 1);
   huge_string[huge_string_len - 1] = '\0';

   collection = get_test_collection (client, "test_upsert_large");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, true, NULL);
   assert (bulk);

   bson_append_document_begin (&doc, "$set", -1, &child);
   assert (bson_append_utf8 (&child, "x", -1, huge_string, huge_string_len));
   bson_append_document_end (&doc, &child);

   mongoc_bulk_operation_update (bulk, sel, &doc, true);
   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 1,"
                         " 'upserted':  [{'index': 0, '_id': 1}],"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 1);

   bson_destroy (sel);
   bson_destroy (&doc);
   bson_destroy (&reply);
   mongoc_bulk_operation_destroy (bulk);
   mongoc_collection_destroy (collection);
   bson_free (huge_string);
   mongoc_client_destroy (client);
}


static void
test_replace_one_ordered ()
{
   test_replace_one (true);
}


static void
test_replace_one_unordered ()
{
   test_replace_one (false);
}


static void
test_update (bool ordered)
{
   mongoc_client_t *client;
   bool has_write_cmds;
   mongoc_collection_t *collection;
   bson_t *docs_inserted[] = {
      BCON_NEW ("a", BCON_INT32 (1)),
      BCON_NEW ("a", BCON_INT32 (2)),
      BCON_NEW ("a", BCON_INT32 (3), "foo", BCON_UTF8 ("bar"))
   };
   int i;
   mongoc_bulk_operation_t *bulk;
   bson_error_t error;
   bson_t reply;
   bson_t *sel;
   bson_t *bad_update_doc = BCON_NEW ("foo", BCON_UTF8 ("bar"));
   bson_t *update_doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "test_update");
   assert (collection);

   for (i = 0; i < sizeof docs_inserted / sizeof (bson_t *); i++) {
      assert (mongoc_collection_insert (collection, MONGOC_INSERT_NONE,
                                        docs_inserted[i], NULL, NULL));
   }

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
   assert (bulk);

   /* update doc without $-operators rejected */
   sel = BCON_NEW ("a", "{", "$gte", BCON_INT32 (2), "}");
   suppress_one_message ();
   mongoc_bulk_operation_update (bulk, sel, bad_update_doc, false);
   ASSERT_CMPINT (0, ==, (int)bulk->commands.len);

   update_doc = BCON_NEW ("$set", "{", "foo", BCON_UTF8 ("bar"), "}");
   mongoc_bulk_operation_update (bulk, sel, update_doc, false);
   r = (bool)mongoc_bulk_operation_execute (bulk, &reply, &error);
   assert (r);

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  2,"
                         " 'nUpserted': 0,"
                         " 'upserted':  {'$exists': false},"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 1);  /* one doc already had "foo": "bar" */

   r = mongoc_collection_drop (collection, &error);
   assert (r);

   mongoc_bulk_operation_destroy (bulk);
   bson_destroy (update_doc);
   bson_destroy (&reply);
   bson_destroy (bad_update_doc);
   bson_destroy (sel);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);

   for (i = 0; i < sizeof docs_inserted / sizeof (bson_t *); i++) {
      bson_destroy (docs_inserted[i]);
   }
}


static void
test_update_ordered (void)
{
   test_update (true);
}


static void
test_update_unordered (void)
{
   test_update (false);
}


static void
test_index_offset (void)
{
   mongoc_bulk_operation_t *bulk;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bool has_write_cmds;
   bson_error_t error;
   bson_t reply;
   bson_t *sel;
   bson_t *doc;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

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

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  1,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 1,"
                         " 'upserted': [{'index': 1, '_id': 1234}],"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 0);

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
test_bulk_edge_case_372 (bool ordered)
{
   mongoc_client_t *client;
   bool has_write_cmds;
   mongoc_collection_t *collection;
   mongoc_bulk_operation_t *bulk;
   bson_error_t error;
   bson_iter_t iter;
   bson_iter_t citer;
   bson_t *selector;
   bson_t *update;
   bson_t reply;
   bool r;

   client = test_framework_client_new (NULL);
   assert (client);
   has_write_cmds = server_has_write_commands (client);

   collection = get_test_collection (client, "CDRIVER_372");
   assert (collection);

   bulk = mongoc_collection_create_bulk_operation (collection, ordered, NULL);
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

   if (has_write_cmds) {
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

   ASSERT_MATCH (&reply, "{'nInserted': 0,"
                         " 'nRemoved':  0,"
                         " 'nMatched':  0,"
                         " 'nUpserted': 3,"
                         " 'upserted': ["
                         "     {'index': 0, '_id': 0},"
                         "     {'index': 1, '_id': 1},"
                         "     {'index': 2, '_id': 2}"
                         " ],"
                         " 'writeErrors': []}");

   check_n_modified (has_write_cmds, &reply, 0);

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
test_bulk_edge_case_372_ordered ()
{
   test_bulk_edge_case_372 (true);
}


static void
test_bulk_edge_case_372_unordered ()
{
   test_bulk_edge_case_372 (false);
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
   TestSuite_Add (suite, "/BulkOperation/insert_ordered", test_insert_ordered);
   TestSuite_Add (suite, "/BulkOperation/insert_unordered",
                  test_insert_unordered);
   TestSuite_Add (suite, "/BulkOperation/insert_check_keys",
                  test_insert_check_keys);
   TestSuite_Add (suite, "/BulkOperation/update_ordered", test_update_ordered);
   TestSuite_Add (suite, "/BulkOperation/update_unordered",
                  test_update_unordered);
   TestSuite_Add (suite, "/BulkOperation/upsert_ordered", test_upsert_ordered);
   TestSuite_Add (suite, "/BulkOperation/upsert_unordered",
                  test_upsert_unordered);
   TestSuite_Add (suite, "/BulkOperation/upsert_large", test_upsert_large);
   TestSuite_Add (suite, "/BulkOperation/update_one_ordered",
                  test_update_one_ordered);
   TestSuite_Add (suite, "/BulkOperation/update_one_unordered",
                  test_update_one_unordered);
   TestSuite_Add (suite, "/BulkOperation/replace_one_ordered",
                  test_replace_one_ordered);
   TestSuite_Add (suite, "/BulkOperation/replace_one_unordered",
                  test_replace_one_unordered);
   TestSuite_Add (suite, "/BulkOperation/index_offset", test_index_offset);
   TestSuite_Add (suite, "/BulkOperation/CDRIVER-372_ordered",
                  test_bulk_edge_case_372_ordered);
   TestSuite_Add (suite, "/BulkOperation/CDRIVER-372_unordered",
                  test_bulk_edge_case_372_unordered);
   TestSuite_Add (suite, "/BulkOperation/new", test_bulk_new);
   TestSuite_Add (suite, "/BulkOperation/over_1000", test_bulk_edge_over_1000);
}
