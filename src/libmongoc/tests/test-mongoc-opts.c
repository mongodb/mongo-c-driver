#include <mongoc.h>

#include "TestSuite.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"
#include "mock_server/mock-rs.h"
#include "test-conveniences.h"


/* kinds of options */
typedef enum {
   OPT_READ_CONCERN,
   OPT_WRITE_CONCERN,
   OPT_READ_PREFS,
} opt_type_t;


/* objects on which options can be set */
typedef enum {
   OPT_SOURCE_NONE = 0,
   OPT_SOURCE_FUNC = 1 << 0,
   OPT_SOURCE_COLL = 1 << 1,
   OPT_SOURCE_DB = 1 << 2,
   OPT_SOURCE_CLIENT = 1 << 3,
} opt_source_t;


typedef struct {
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *collection;
   const mongoc_read_prefs_t *prefs;
   const bson_t *opts;
   /* find, aggregate, etc. store the cursor here while running */
   mongoc_cursor_t *cursor;
   /* find, etc. store the cursor result here, insert_many stages input here */
   const bson_t *doc;
   bson_error_t error;
} func_ctx_t;


typedef future_t *(func_with_opts_t) (func_ctx_t *ctx, const char **cmd_name);


typedef struct {
   opt_type_t opt_type;
   opt_source_t opt_source;
   func_with_opts_t *func_with_opts;
   const char *func_name;
   int n_sections;
} opt_inheritance_test_t;


static void
func_ctx_init (func_ctx_t *ctx,
               mongoc_client_t *client,
               mongoc_database_t *db,
               mongoc_collection_t *collection,
               const mongoc_read_prefs_t *prefs,
               const bson_t *opts)
{
   ctx->client = client;
   ctx->db = db;
   ctx->collection = collection;
   ctx->prefs = prefs;
   ctx->opts = opts;
   ctx->cursor = NULL;
   ctx->doc = NULL;
   memset (&ctx->error, sizeof (ctx->error), 0);
}


static void
func_ctx_cleanup (func_ctx_t *ctx)
{
   mongoc_cursor_destroy (ctx->cursor);
}


/* if type is e.g. "collection", set readConcern level collection, writeConcern
 * w=collection, readPreference tags [{collection: "yes"}] */
#define SET_OPT_PREAMBLE(_type)                                                \
   mongoc_read_concern_t *rc = mongoc_read_concern_new ();                     \
   mongoc_write_concern_t *wc = mongoc_write_concern_new ();                   \
   mongoc_read_prefs_t *prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY); \
                                                                               \
   mongoc_read_concern_set_level (rc, #_type);                                 \
   mongoc_write_concern_set_wtag (wc, #_type);                                 \
   mongoc_read_prefs_set_tags (prefs, tmp_bson ("[{'%s': 'yes'}]", #_type))

#define SET_OPT_CLEANUP               \
   mongoc_read_concern_destroy (rc);  \
   mongoc_write_concern_destroy (wc); \
   mongoc_read_prefs_destroy (prefs);

#define SET_OPT(_type)                                     \
   static void set_##_type##_opt (mongoc_##_type##_t *obj, \
                                  opt_type_t opt_type)     \
   {                                                       \
      SET_OPT_PREAMBLE (_type);                            \
                                                           \
      switch (opt_type) {                                  \
      case OPT_READ_CONCERN:                               \
         mongoc_##_type##_set_read_concern (obj, rc);      \
         break;                                            \
      case OPT_WRITE_CONCERN:                              \
         mongoc_##_type##_set_write_concern (obj, wc);     \
         break;                                            \
      case OPT_READ_PREFS:                                 \
         mongoc_##_type##_set_read_prefs (obj, prefs);     \
         break;                                            \
      default:                                        \
         abort ();                                         \
      }                                                    \
                                                           \
      SET_OPT_CLEANUP;                                     \
   }

SET_OPT (client)
SET_OPT (database)
SET_OPT (collection)


static void
set_func_opt (bson_t *opts,
              mongoc_read_prefs_t **prefs_ptr,
              opt_type_t opt_type)
{
   SET_OPT_PREAMBLE (function);

   switch (opt_type) {
   case OPT_READ_CONCERN:
      BSON_ASSERT (mongoc_read_concern_append (rc, opts));
      break;
   case OPT_WRITE_CONCERN:
      BSON_ASSERT (mongoc_write_concern_append (wc, opts));
      break;
   case OPT_READ_PREFS:
      *prefs_ptr = mongoc_read_prefs_copy (prefs);
      break;
   default:
      abort ();
   }

   SET_OPT_CLEANUP;
}


/* return the JSON fragment we expect to be included in a command due to an
 * inherited option. e.g., when "count" inherits readConcern from the DB, it
 * should include readConcern: {level: 'database'} in the command body. */
static char *
opt_json (const char *option_source, opt_type_t opt_type)
{
   switch (opt_type) {
   case OPT_READ_CONCERN:
      return bson_strdup_printf ("'readConcern': {'level': '%s'}",
                                 option_source);
   case OPT_WRITE_CONCERN:
      return bson_strdup_printf ("'writeConcern': {'w': '%s'}", option_source);
   case OPT_READ_PREFS:
      return bson_strdup_printf (
         "'$readPreference': {'mode': 'secondary', 'tags': [{'%s': 'yes'}]}",
         option_source);
   default:
      abort ();
   }
}


static const char *
opt_type_name (opt_type_t opt_type)
{
   switch (opt_type) {
   case OPT_READ_CONCERN:
      return "readConcern";
   case OPT_WRITE_CONCERN:
      return "writeConcern";
   case OPT_READ_PREFS:
      return "readPrefs";
   default:
      abort ();
   }
}


/* func_with_opts_t implementations */
static future_t *
insert_one (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "insert";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_insert_one (
      ctx->collection, tmp_bson ("{}"), ctx->opts, NULL, &ctx->error);
}


static future_t *
insert_many (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "insert";

   BSON_ASSERT (!ctx->prefs);

   /* the "array" of input documents must be a valid pointer, stage it here */
   ctx->doc = tmp_bson ("{}");
   return future_collection_insert_many (
      ctx->collection, &ctx->doc, 1, ctx->opts, NULL, &ctx->error);
}


static future_t *
update_one (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "update";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_update_one (ctx->collection,
                                        tmp_bson ("{}"),
                                        tmp_bson ("{}"),
                                        ctx->opts,
                                        NULL,
                                        &ctx->error);
}


static future_t *
update_many (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "update";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_update_many (ctx->collection,
                                         tmp_bson ("{}"),
                                         tmp_bson ("{}"),
                                         ctx->opts,
                                         NULL,
                                         &ctx->error);
}


static future_t *
replace_one (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "update";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_replace_one (ctx->collection,
                                         tmp_bson ("{}"),
                                         tmp_bson ("{}"),
                                         ctx->opts,
                                         NULL,
                                         &ctx->error);
}


static future_t *
delete_one (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "delete";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_delete_one (
      ctx->collection, tmp_bson ("{}"), ctx->opts, NULL, &ctx->error);
}


static future_t *
delete_many (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "delete";
   BSON_ASSERT (!ctx->prefs);
   return future_collection_delete_many (
      ctx->collection, tmp_bson ("{}"), ctx->opts, NULL, &ctx->error);
}


static future_t *
find (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "find";
   ctx->cursor = mongoc_collection_find_with_opts (
      ctx->collection, tmp_bson ("{}"), ctx->opts, ctx->prefs);

   return future_cursor_next (ctx->cursor, &ctx->doc);
}


static future_t *
count (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "count";
   return future_collection_count_with_opts (ctx->collection,
                                             MONGOC_QUERY_NONE,
                                             NULL,
                                             0,
                                             0,
                                             ctx->opts,
                                             ctx->prefs,
                                             &ctx->error);
}


static future_t *
estimated_document_count (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "count";
   return future_collection_estimated_document_count (
      ctx->collection, ctx->opts, ctx->prefs, NULL, &ctx->error);
}


static future_t *
count_documents (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "aggregate";
   return future_collection_count_documents (ctx->collection,
                                             tmp_bson ("{}"),
                                             ctx->opts,
                                             ctx->prefs,
                                             NULL,
                                             &ctx->error);
}


static future_t *
aggregate (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "aggregate";
   ctx->cursor =
      mongoc_collection_aggregate (ctx->collection,
                                   MONGOC_QUERY_NONE,
                                   tmp_bson ("{'pipeline': [{'$out': 'foo'}]}"),
                                   ctx->opts,
                                   ctx->prefs);

   return future_cursor_next (ctx->cursor, &ctx->doc);
}


static future_t *
collection_read_cmd (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "foo";
   return future_collection_read_command_with_opts (
      ctx->collection,
      tmp_bson ("{'foo': 'collection'}"),
      ctx->prefs,
      ctx->opts,
      NULL,
      &ctx->error);
}


static future_t *
collection_write_cmd (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "foo";
   return future_collection_write_command_with_opts (
      ctx->collection,
      tmp_bson ("{'foo': 'collection'}"),
      ctx->opts,
      NULL,
      &ctx->error);
}


static future_t *
client_read_write_cmd (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "foo";
   return future_client_read_write_command_with_opts (
      ctx->client,
      "db",
      tmp_bson ("{'foo': 'collection'}"),
      ctx->prefs,
      ctx->opts,
      NULL,
      &ctx->error);
}


static future_t *
db_read_write_cmd (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "foo";
   return future_database_read_write_command_with_opts (
      ctx->db,
      tmp_bson ("{'foo': 'collection'}"),
      ctx->prefs,
      ctx->opts,
      NULL,
      &ctx->error);
}


static future_t *
collection_read_write_cmd (func_ctx_t *ctx, const char **cmd_name)
{
   *cmd_name = "foo";
   return future_collection_read_write_command_with_opts (
      ctx->collection,
      tmp_bson ("{'foo': 'collection'}"),
      ctx->prefs,
      ctx->opts,
      NULL,
      &ctx->error);
}


static void
test_func_inherits_opts (void *ctx)
{
   opt_inheritance_test_t *test = (opt_inheritance_test_t *) ctx;

   /* for example, test mongoc_collection_find_with_opts with no read pref,
    * with a read pref set on the collection (OPT_SOURCE_COLL), with an explicit
    * read pref (OPT_SOURCE_FUNC), or with one read pref on the collection and
    * a different one passed explicitly */
   opt_source_t source_matrix[] = {OPT_SOURCE_NONE,
                                   test->opt_source,
                                   OPT_SOURCE_FUNC,
                                   test->opt_source | OPT_SOURCE_FUNC};

   size_t i;
   func_ctx_t func_ctx;
   mock_rs_t *rs;
   const char *option_source;
   mongoc_client_t *client;
   mongoc_database_t *db;
   mongoc_collection_t *collection;
   bson_t opts = BSON_INITIALIZER;
   mongoc_read_prefs_t *func_prefs = NULL;
   future_t *future;
   request_t *request;
   const char *cmd_name;
   bson_t *cmd;
   bool expect_secondary;
   bson_error_t error;

   /* one primary, one secondary */
   rs = mock_rs_with_autoismaster (WIRE_VERSION_OP_MSG, true, 1, 0);
   /* we use read pref tags like "collection": "yes" to verify where the
    * pref was inherited from; ensure all secondaries match all tags */
   mock_rs_tag_secondary (rs,
                          0,
                          tmp_bson ("{'client': 'yes',"
                                    " 'database': 'yes',"
                                    " 'collection': 'yes',"
                                    " 'function': 'yes'}"));

   mock_rs_run (rs);

   /* iterate over all combinations of options sources: e.g., an option set on
    * collection and not function, on function not collection, both, neither */
   for (i = 0; i < sizeof (source_matrix) / (sizeof (opt_source_t)); i++) {
      option_source = NULL;
      expect_secondary = false;
      func_prefs = NULL;
      bson_reinit (&opts);

      client = mongoc_client_new_from_uri (mock_rs_get_uri (rs));
      if (source_matrix[i] & OPT_SOURCE_CLIENT) {
         set_client_opt (client, test->opt_type);
         option_source = "client";
      }

      db = mongoc_client_get_database (client, "db");
      if (source_matrix[i] & OPT_SOURCE_DB) {
         set_database_opt (db, test->opt_type);
         option_source = "database";
      }

      collection = mongoc_database_get_collection (db, "collection");
      if (source_matrix[i] & OPT_SOURCE_COLL) {
         set_collection_opt (collection, test->opt_type);
         option_source = "collection";
      }

      if (source_matrix[i] & OPT_SOURCE_FUNC) {
         set_func_opt (&opts, &func_prefs, test->opt_type);
         option_source = "function";
      }

      func_ctx_init (&func_ctx, client, db, collection, func_prefs, &opts);
      future = test->func_with_opts (&func_ctx, &cmd_name);

      if (source_matrix[i] != OPT_SOURCE_NONE) {
         char *tmp_json = opt_json (option_source, test->opt_type);
         cmd = tmp_bson ("{'%s': 'collection', %s}", cmd_name, tmp_json);
         bson_free (tmp_json);

         if (test->opt_type == OPT_READ_PREFS) {
            expect_secondary = true;
         }
      } else {
         cmd = tmp_bson ("{'%s': 'collection'}", cmd_name);
      }

      /* write commands send two OP_MSG sections */
      if (test->n_sections == 2) {
         request = mock_rs_receives_msg (rs, 0, cmd, tmp_bson ("{}"));
      } else {
         request = mock_rs_receives_msg (rs, 0, cmd);
      }

      if (expect_secondary) {
         BSON_ASSERT (mock_rs_request_is_to_secondary (rs, request));
      } else {
         BSON_ASSERT (mock_rs_request_is_to_primary (rs, request));
      }

      if (func_ctx.cursor) {
         mock_server_replies_simple (request,
                                     "{'ok': 1,"
                                     " 'cursor': {"
                                     "    'id': 0,"
                                     "    'ns': 'db.collection',"
                                     "    'firstBatch': []}}");

         BSON_ASSERT (!future_get_bool (future));
         ASSERT_OR_PRINT (!mongoc_cursor_error (func_ctx.cursor, &error),
                          error);
      } else {
         mock_server_replies_simple (request, "{'ok': 1}");
         future_wait (future);
      }

      future_destroy (future);
      request_destroy (request);
      mongoc_read_prefs_destroy (func_prefs);
      func_ctx_cleanup (&func_ctx);
      mongoc_collection_destroy (collection);
      mongoc_database_destroy (db);
      mongoc_client_destroy (client);
   }

   bson_destroy (&opts);
   mock_rs_destroy (rs);
}


/* commands that send one OP_MSG section */
#define OPT_TEST(_opt_type, _opt_source, _func)                   \
   {                                                              \
      OPT_##_opt_type, OPT_SOURCE_##_opt_source, _func, #_func, 1 \
   }

/* commands that send two OP_MSG sections */
#define OPT_WRITE_TEST(_opt_type, _opt_source, _func)             \
   {                                                              \
      OPT_##_opt_type, OPT_SOURCE_##_opt_source, _func, #_func, 2 \
   }


static opt_inheritance_test_t gInheritanceTests[] = {
   OPT_TEST (READ_CONCERN, COLL, find),
   OPT_TEST (READ_PREFS, COLL, find),

   OPT_TEST (READ_CONCERN, COLL, count),
   OPT_TEST (READ_PREFS, COLL, count),

   OPT_TEST (READ_CONCERN, COLL, estimated_document_count),
   OPT_TEST (READ_PREFS, COLL, estimated_document_count),

   OPT_TEST (READ_CONCERN, COLL, count_documents),
   OPT_TEST (READ_PREFS, COLL, count_documents),

   OPT_TEST (READ_CONCERN, COLL, aggregate),
   OPT_TEST (WRITE_CONCERN, COLL, aggregate),
   OPT_TEST (READ_PREFS, COLL, aggregate),

   OPT_TEST (READ_CONCERN, COLL, collection_read_cmd),
   OPT_TEST (READ_PREFS, COLL, collection_read_cmd),
   OPT_TEST (WRITE_CONCERN, COLL, collection_write_cmd),

   /* read_write_command functions deliberately ignore read prefs */
   OPT_TEST (READ_CONCERN, CLIENT, client_read_write_cmd),
   OPT_TEST (WRITE_CONCERN, CLIENT, client_read_write_cmd),

   OPT_TEST (READ_CONCERN, DB, db_read_write_cmd),
   OPT_TEST (WRITE_CONCERN, DB, db_read_write_cmd),

   OPT_TEST (READ_CONCERN, COLL, collection_read_write_cmd),
   OPT_TEST (WRITE_CONCERN, COLL, collection_read_write_cmd),

   OPT_WRITE_TEST (WRITE_CONCERN, COLL, insert_one),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, insert_many),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, update_one),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, update_many),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, replace_one),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, delete_one),
   OPT_WRITE_TEST (WRITE_CONCERN, COLL, delete_many),
};


static void
install_inheritance_tests (TestSuite *suite,
                           opt_inheritance_test_t *tests,
                           size_t n)
{
   size_t i;
   opt_inheritance_test_t *test;
   char *name;

   for (i = 0; i < n; i++) {
      test = &tests[i];
      name = bson_strdup_printf (
         "/inheritance/%s/%s", test->func_name, opt_type_name (test->opt_type));

      TestSuite_AddFull (suite,
                         name,
                         test_func_inherits_opts,
                         NULL,
                         test,
                         TestSuite_CheckMockServerAllowed);

      bson_free (name);
   }
}


void
test_opts_install (TestSuite *suite)
{
   install_inheritance_tests (suite,
                              gInheritanceTests,
                              sizeof (gInheritanceTests) /
                                 sizeof (opt_inheritance_test_t));
}
