#include "mongoc.h"
#include "mongoc-host-list-private.h"
#include "mongoc-thread-private.h"

#include "json-test.h"
#include "test-libmongoc.h"


typedef struct {
   mongoc_mutex_t mutex;
   mongoc_host_list_t *hosts;
} context_t;


static void
topology_changed (const mongoc_apm_topology_changed_t *event)
{
   context_t *ctx;
   const mongoc_topology_description_t *td;
   size_t i;
   size_t n;
   mongoc_server_description_t **sds;

   ctx = (context_t *) mongoc_apm_topology_changed_get_context (event);

   td = mongoc_apm_topology_changed_get_new_description (event);
   sds = mongoc_topology_description_get_servers (td, &n);

   mongoc_mutex_lock (&ctx->mutex);
   for (i = 0; i < n; i++) {
      ctx->hosts = _mongoc_host_list_push (
         sds[i]->host.host, sds[i]->host.port, AF_UNSPEC, ctx->hosts);
   }
   mongoc_mutex_unlock (&ctx->mutex);

   mongoc_server_descriptions_destroy_all (sds, n);
}


static bool
host_list_contains (const mongoc_host_list_t *hl, const char *host_and_port)
{
   while (hl) {
      if (!strcmp (hl->host_and_port, host_and_port)) {
         return true;
      }

      hl = hl->next;
   }

   return false;
}


static int
hosts_count (const bson_t *test)
{
   bson_iter_t iter;
   bson_iter_t hosts;
   int c = 0;

   BSON_ASSERT (bson_iter_init_find (&iter, test, "hosts"));
   BSON_ASSERT (bson_iter_recurse (&iter, &hosts));
   while (bson_iter_next (&hosts)) {
      c++;
   }

   return c;
}


static bool
_host_list_matches (const bson_t *test, mongoc_client_t *client, context_t *ctx)
{
   bson_iter_t iter;
   bson_iter_t hosts;
   bool r;
   bson_error_t error;
   const char *host_and_port;
   bool ret = true;

   BSON_ASSERT (bson_iter_init_find (&iter, test, "hosts"));
   BSON_ASSERT (bson_iter_recurse (&iter, &hosts));

   r = mongoc_client_command_simple (
      client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
   ASSERT_OR_PRINT (r, error);

   mongoc_mutex_lock (&ctx->mutex);
   BSON_ASSERT (bson_iter_recurse (&iter, &hosts));
   while (bson_iter_next (&hosts)) {
      host_and_port = bson_iter_utf8 (&hosts, NULL);
      if (!host_list_contains (ctx->hosts, host_and_port)) {
         ret = false;
         break;
      }
   }

   _mongoc_host_list_destroy_all (ctx->hosts);
   ctx->hosts = NULL;
   mongoc_mutex_unlock (&ctx->mutex);

   return ret;
}


static void
_test_srv_maybe_pooled (bson_t *test, bool pooled)
{
   context_t ctx;
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   int n_hosts;
   bson_error_t error;
   bool r;

   mongoc_mutex_init (&ctx.mutex);
   ctx.hosts = NULL;

   uri = mongoc_uri_new_with_error (bson_lookup_utf8 (test, "uri"), &error);
   ASSERT_OR_PRINT (uri, error);
   mongoc_uri_set_option_as_int32 (uri, "heartbeatFrequencyMS", 500);

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_topology_changed_cb (callbacks, topology_changed);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      test_framework_set_pool_ssl_opts (pool);
      mongoc_client_pool_set_apm_callbacks (pool, callbacks, &ctx);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
      test_framework_set_ssl_opts (client);
      mongoc_client_set_apm_callbacks (client, callbacks, &ctx);
   }

   n_hosts = hosts_count (test);

   if (n_hosts) {
      r = mongoc_client_command_simple (
         client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
      ASSERT_OR_PRINT (r, error);
      WAIT_UNTIL (_host_list_matches (test, client, &ctx));
   } else {
      r = mongoc_client_command_simple (
         client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
      BSON_ASSERT (!r);
      ASSERT_ERROR_CONTAINS (error,
                             MONGOC_ERROR_SERVER_SELECTION,
                             MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                             mongoc_uri_get_service (uri));
   }

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_apm_callbacks_destroy (callbacks);
   mongoc_uri_destroy (uri);
}


static void
test_srv (bson_t *test)
{
   _test_srv_maybe_pooled (test, false);
   _test_srv_maybe_pooled (test, true);
}


static int
test_srv_check (void)
{
   return test_framework_getenv_bool ("MONGOC_TEST_SRV") ? 1 : 0;
}


/*
 *-----------------------------------------------------------------------
 *
 * Runner for the JSON tests for mongodb+srv URIs.
 *
 *-----------------------------------------------------------------------
 */
static void
test_all_spec_tests (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (realpath (JSON_DIR "/srv", resolved));
   install_json_test_suite_with_check (
      suite, resolved, test_srv, test_srv_check);
}


void
test_srv_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
}
