#include <fcntl.h>
#include <mongoc.h>

#include "mongoc-client-private.h"
#include "mongoc-cursor-private.h"
#include "mongoc-uri-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"
#include "mongoc-tests.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "exhaust-test"

static mongoc_collection_t *
get_test_collection (mongoc_client_t *client,
                     const char      *name)
{
   mongoc_collection_t *ret;
   char *str;

   str = gen_collection_name (name);
   ret = mongoc_client_get_collection (client, "test", str);
   bson_free (str);

   return ret;
}

int skip_if_mongos (void)
{
   return test_framework_is_mongos () ? 0 : 1;
}


static void
test_exhaust_cursor (void *context)
{
   mongoc_stream_t *stream;
   mongoc_write_concern_t *wr;
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   mongoc_cursor_t *cursor2;
   const bson_t *doc;
   bson_t q;
   bson_t b[10];
   bson_t *bptr[10];
   int i;
   bool r;
   uint32_t hint;
   bson_error_t error;
   bson_oid_t oid;

   client = test_framework_client_new ();
   assert (client);

   collection = get_test_collection (client, "test_exhaust_cursor");
   assert (collection);

   mongoc_collection_drop(collection, &error);

   wr = mongoc_write_concern_new ();
   mongoc_write_concern_set_journal (wr, true);

   /* bulk insert some records to work on */
   {
      bson_init(&q);

      for (i = 0; i < 10; i++) {
         bson_init(&b[i]);
         bson_oid_init(&oid, NULL);
         bson_append_oid(&b[i], "_id", -1, &oid);
         bson_append_int32(&b[i], "n", -1, i % 2);
         bptr[i] = &b[i];
      }

      BEGIN_IGNORE_DEPRECATIONS;
      ASSERT_OR_PRINT (mongoc_collection_insert_bulk (
                          collection, MONGOC_INSERT_NONE,
                          (const bson_t **)bptr, 10, wr, &error),
                       error);
      END_IGNORE_DEPRECATIONS;
   }

   /* create a couple of cursors */
   {
      cursor = mongoc_collection_find (collection, MONGOC_QUERY_EXHAUST, 0, 0, 0, &q,
                                       NULL, NULL);

      cursor2 = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                        NULL, NULL);
   }

   /* Read from the exhaust cursor, ensure that we're in exhaust where we
    * should be and ensure that an early destroy properly causes a disconnect
    * */
   {
      uint32_t local_hint;

      r = mongoc_cursor_next (cursor, &doc);
      if (!r) {
         mongoc_cursor_error (cursor, &error);
         printf ("cursor error: %s\n", error.message);
      }
      assert (r);
      assert (doc);
      assert (cursor->in_exhaust);
      assert (client->in_exhaust);
      local_hint = cursor->hint;

      /* destroy the cursor, make sure a disconnect happened */
      mongoc_cursor_destroy (cursor);
      stream = (mongoc_stream_t *)mongoc_set_get(client->cluster.nodes, local_hint);
      assert (! stream);

      assert (! client->in_exhaust);
   }

   /* Grab a new exhaust cursor, then verify that reading from that cursor
    * (putting the client into exhaust), breaks a mid-stream read from a
    * regular cursor */
   {
      cursor = mongoc_collection_find (collection, MONGOC_QUERY_EXHAUST, 0, 0, 0, &q,
                                       NULL, NULL);

      for (i = 0; i < 5; i++) {
         r = mongoc_cursor_next (cursor2, &doc);
         if (!r) {
            mongoc_cursor_error (cursor2, &error);
            printf ("cursor error: %s\n", error.message);
         }
         assert (r);
         assert (doc);
      }

      r = mongoc_cursor_next (cursor, &doc);
      assert (r);
      assert (doc);

      doc = NULL;
      r = mongoc_cursor_next (cursor2, &doc);
      assert (!r);
      assert (!doc);

      mongoc_cursor_error(cursor2, &error);
      assert (error.domain == MONGOC_ERROR_CLIENT);
      assert (error.code == MONGOC_ERROR_CLIENT_IN_EXHAUST);

      mongoc_cursor_destroy (cursor2);
   }

   /* make sure writes fail as well */
   {
      BEGIN_IGNORE_DEPRECATIONS;
      r = mongoc_collection_insert_bulk (collection, MONGOC_INSERT_NONE,
                                         (const bson_t **)bptr, 10, wr, &error);
      END_IGNORE_DEPRECATIONS;

      assert (!r);
      assert (error.domain == MONGOC_ERROR_CLIENT);
      assert (error.code == MONGOC_ERROR_CLIENT_IN_EXHAUST);
   }

   /* we're still in exhaust.
    *
    * 1. check that we can create a new cursor, as long as we don't read from it
    * 2. fully exhaust the exhaust cursor
    * 3. make sure that we don't disconnect at destroy
    * 4. make sure we can read the cursor we made during the exhuast
    */
   {
      cursor2 = mongoc_collection_find (collection, MONGOC_QUERY_NONE, 0, 0, 0, &q,
                                        NULL, NULL);

      stream = (mongoc_stream_t *)mongoc_set_get(client->cluster.nodes, cursor->hint);
      hint = cursor->hint;

      for (i = 1; i < 10; i++) {
         r = mongoc_cursor_next (cursor, &doc);
         assert (r);
         assert (doc);
      }

      r = mongoc_cursor_next (cursor, &doc);
      assert (!r);
      assert (!doc);

      mongoc_cursor_destroy (cursor);

      assert (stream == (mongoc_stream_t *)mongoc_set_get(client->cluster.nodes, hint));

      r = mongoc_cursor_next (cursor2, &doc);
      assert (r);
      assert (doc);
   }

   bson_destroy(&q);
   for (i = 0; i < 10; i++) {
      bson_destroy(&b[i]);
   }

   ASSERT_OR_PRINT (mongoc_collection_drop (collection, &error), error);

   mongoc_write_concern_destroy (wr);
   mongoc_cursor_destroy (cursor2);
   mongoc_collection_destroy(collection);
   mongoc_client_destroy (client);
}


void
test_exhaust_install (TestSuite *suite)
{
   TestSuite_AddFull (suite, "/Client/exhaust_cursor", test_exhaust_cursor, NULL, NULL, skip_if_mongos);
}

