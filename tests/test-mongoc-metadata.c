/*
 * Copyright 2016 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mongoc.h>
#include "mongoc-client-private.h"
#include "mongoc-metadata.h"
#include "mongoc-metadata-private.h"

#include "TestSuite.h"
#include "test-libmongoc.h"
#include "test-conveniences.h"
#include "mock_server/future.h"
#include "mock_server/future-functions.h"
#include "mock_server/mock-server.h"

/*
 * Call this before any test which uses mongoc_metadata_append, to
 * reset the global state and unfreeze the metadata struct. Call it
 * after a test so later tests don't have a weird metadata document
 *
 * This is not safe to call while we have any clients or client pools running!
 */
static void
_reset_metadata (void)
{
   _mongoc_metadata_cleanup ();
   _mongoc_metadata_init ();
}

static void
test_mongoc_metadata_append_success (void)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;
   request_t *request;
   const bson_t *request_doc;
   bson_iter_t iter;
   bson_iter_t md_iter;
   bson_iter_t inner_iter;
   const char *val;

   const char *driver_name = "php driver";
   const char *driver_version = "version abc";
   const char *platform = "./configure -nottoomanyflags";

   char big_string [METADATA_MAX_SIZE];

   memset (big_string, 'a', METADATA_MAX_SIZE - 1);
   big_string [METADATA_MAX_SIZE - 1] = '\0';

   _reset_metadata ();
   /* Make sure setting the metadata works */
   ASSERT (mongoc_metadata_append (driver_name, driver_version, platform));

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", 500);
   pool = mongoc_client_pool_new (uri);

   /* Force topology scanner to start */
   client = mongoc_client_pool_pop (pool);

   request = mock_server_receives_ismaster (server);
   ASSERT (request);
   request_doc = request_get_doc (request, 0);
   ASSERT (request_doc);
   ASSERT (bson_has_field (request_doc, "isMaster"));
   ASSERT (bson_has_field (request_doc, METADATA_FIELD));

   ASSERT (bson_iter_init_find (&iter, request_doc, METADATA_FIELD));
   ASSERT (bson_iter_recurse (&iter, &md_iter));

   /* Make sure driver.name and driver.version and platform are all right */
   ASSERT (bson_iter_find (&md_iter, "driver"));
   ASSERT (BSON_ITER_HOLDS_DOCUMENT (&md_iter));
   ASSERT (bson_iter_recurse (&md_iter, &inner_iter));
   ASSERT (bson_iter_find (&inner_iter, "name"));
   ASSERT (BSON_ITER_HOLDS_UTF8 (&inner_iter));
   val = bson_iter_utf8 (&inner_iter, NULL);
   ASSERT (val);
   ASSERT (strstr (val, driver_name) != NULL);

   ASSERT (bson_iter_find (&inner_iter, "version"));
   ASSERT (BSON_ITER_HOLDS_UTF8 (&inner_iter));
   val = bson_iter_utf8 (&inner_iter, NULL);
   ASSERT (val);
   ASSERT (strstr (val, driver_version));

   /* Check os type not empty */
   ASSERT (bson_iter_find (&md_iter, "os"));
   ASSERT (BSON_ITER_HOLDS_DOCUMENT (&md_iter));
   ASSERT (bson_iter_recurse (&md_iter, &inner_iter));

   ASSERT (bson_iter_find (&inner_iter, "type"));
   ASSERT (BSON_ITER_HOLDS_UTF8 (&inner_iter));
   val = bson_iter_utf8 (&inner_iter, NULL);
   ASSERT (val);
   ASSERT (strlen (val) > 0);

   /* Not checking os_name, as the spec says it can be NULL. */

   /* Check platform field ok */
   ASSERT (bson_iter_find (&md_iter, "platform"));
   ASSERT (BSON_ITER_HOLDS_UTF8 (&md_iter));
   val = bson_iter_utf8 (&md_iter, NULL);
   ASSERT (val);
   ASSERT (strstr (val, platform) != NULL);

   mock_server_replies_simple (request,
                               "{'ok': 1, 'ismaster': true}");
   request_destroy (request);

   /* Cleanup */
   mongoc_client_pool_push (pool, client);
   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);

   _reset_metadata ();
}

static void
test_mongoc_metadata_append_after_cmd (void)
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   mongoc_uri_t *uri;

   _reset_metadata ();

   uri = mongoc_uri_new ("mongodb://127.0.0.1?maxpoolsize=1&minpoolsize=1");
   pool = mongoc_client_pool_new (uri);

   /* Make sure that after we pop a client we can't set global metadata */
   pool = mongoc_client_pool_new (uri);

   client = mongoc_client_pool_pop (pool);

   ASSERT (!mongoc_metadata_append ("a", "a", "a"));

   mongoc_client_pool_push (pool, client);

   mongoc_uri_destroy (uri);
   mongoc_client_pool_destroy (pool);

   _reset_metadata ();
}

/*
 * Append to the platform field a huge string
 * Make sure that it gets truncated
 */
static void
test_mongoc_metadata_too_big (void)
{
   mongoc_client_t *client;
   mock_server_t *server;
   mongoc_uri_t *uri;
   future_t *future;
   request_t *request;
   bson_t *ismaster_doc;
   bson_iter_t iter;

   enum { BUFFER_SIZE = METADATA_MAX_SIZE };
   char big_string[BUFFER_SIZE];
   uint32_t len;
   const uint8_t *dummy;

   server = mock_server_new ();
   mock_server_auto_ismaster (server, "{'ok': 1, 'ismaster': true}");
   mock_server_run (server);

   _reset_metadata ();

   memset (big_string, 'a', BUFFER_SIZE - 1);
   big_string[BUFFER_SIZE - 1] = '\0';
   ASSERT (mongoc_metadata_append (NULL, NULL, big_string));

   uri = mongoc_uri_copy (mock_server_get_uri (server));
   client = mongoc_client_new_from_uri (uri);

   ASSERT (mongoc_client_set_appname (client, "my app"));

   /* Send a ping, mock server deals with it */
   future = future_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'ping': 1}"),
                                          NULL,
                                          NULL,
                                          NULL);
   request = mock_server_receives_command (server, "admin",
                                           MONGOC_QUERY_SLAVE_OK,
                                           "{'ping': 1}");
   mock_server_replies_simple (request, "{'ok': 1}");

   ASSERT (future_get_bool (future));

   /* Make sure the client's isMaster with metadata isn't too big */
   ismaster_doc = &client->topology->scanner->ismaster_cmd_with_metadata,
   bson_iter_init_find (&iter,
                        ismaster_doc,
                        METADATA_FIELD);
   ASSERT (BSON_ITER_HOLDS_DOCUMENT (&iter));
   bson_iter_document (&iter, &len, &dummy);

   /* Should truncate the platform field so we fit exactly */
   ASSERT (len == METADATA_MAX_SIZE);

   future_destroy (future);
   request_destroy (request);
   mongoc_client_destroy (client);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);

   /* So later tests don't have "aaaaa..." as the md platform string */
   _reset_metadata ();
}

/* Test the case where we can't prevent the metadata doc being too big
 * and so we just don't send it */
static void
test_mongoc_metadata_cannot_send (void)
{
   mock_server_t *server;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_client_pool_t *pool;
   request_t *request;
   const char *const server_reply = "{'ok': 1, 'ismaster': true}";
   const bson_t *request_doc;
   char big_string[METADATA_MAX_SIZE];
   mongoc_metadata_t *md;

   _reset_metadata ();

   /* Mess with our global metadata struct so the metadata doc will be
    * way too big */
   memset (big_string, 'a', METADATA_MAX_SIZE - 1);
   big_string[METADATA_MAX_SIZE - 1] = '\0';

   md = _mongoc_metadata_get ();
   bson_free (md->os_name);
   md->os_name = bson_strdup (big_string);

   server = mock_server_new ();
   mock_server_run (server);
   uri = mongoc_uri_copy (mock_server_get_uri (server));
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", 500);
   pool = mongoc_client_pool_new (uri);

   /* Pop a client to trigger the topology scanner */
   client = mongoc_client_pool_pop (pool);
   request = mock_server_receives_ismaster (server);

   /* Make sure the isMaster request DOESN'T have a metadata field: */
   ASSERT (request);
   request_doc = request_get_doc (request, 0);
   ASSERT (request_doc);
   ASSERT (bson_has_field (request_doc, "isMaster"));
   ASSERT (!bson_has_field (request_doc, METADATA_FIELD));

   mock_server_replies_simple (request, server_reply);
   request_destroy (request);

   /* Cause failure on client side */
   request = mock_server_receives_ismaster (server);
   ASSERT (request);
   mock_server_hangs_up (request);
   request_destroy (request);

   /* Make sure the isMaster request still DOESN'T have a metadata field
    * on subsequent heartbeats. */
   request = mock_server_receives_ismaster (server);
   ASSERT (request);
   request_doc = request_get_doc (request, 0);
   ASSERT (request_doc);
   ASSERT (bson_has_field (request_doc, "isMaster"));
   ASSERT (!bson_has_field (request_doc, METADATA_FIELD));

   mock_server_replies_simple (request, server_reply);
   request_destroy (request);

   /* cleanup */
   mongoc_client_pool_push (pool, client);

   mongoc_client_pool_destroy (pool);
   mongoc_uri_destroy (uri);
   mock_server_destroy (server);

   /* Reset again so the next tests don't have a metadata doc which
    * is too big */
   _reset_metadata ();
}

void
test_metadata_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/ClientMetadata/success",
                  test_mongoc_metadata_append_success);
   TestSuite_Add (suite, "/ClientMetadata/failure",
                  test_mongoc_metadata_append_after_cmd);
   TestSuite_Add (suite, "/ClientMetadata/too_big",
                  test_mongoc_metadata_too_big);
   TestSuite_Add (suite, "/ClientMetadata/cannot_send",
                  test_mongoc_metadata_cannot_send);
}
