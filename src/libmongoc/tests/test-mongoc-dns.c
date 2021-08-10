#include <mongoc/mongoc-util-private.h>
#include <mongoc/mongoc-client-pool-private.h>
#include "mongoc/mongoc.h"
#include "mongoc/mongoc-host-list-private.h"
#ifdef MONGOC_ENABLE_SSL
#include "mongoc/mongoc-ssl.h"
#include "mongoc/mongoc-ssl-private.h"
#endif
#include "mongoc/mongoc-thread-private.h"
#include "mongoc/mongoc-uri-private.h"

#include "json-test.h"
#include "test-libmongoc.h"

static void
_assert_options_match (const bson_t *test, mongoc_uri_t *uri)
{
   match_ctx_t ctx = {{0}};
   bson_iter_t iter;
   bson_t opts_from_test;
   const bson_t *opts_from_uri;
   const bson_t *creds_from_uri;
   const bson_t *opts_or_creds;
   bson_iter_t test_opts_iter;
   bson_iter_t uri_opts_iter;
   const char *opt_name, *opt_name_canon;
   const bson_value_t *test_value, *uri_value;

   if (!bson_iter_init_find (&iter, test, "options")) {
      /* no URI options specified in the test */
      return;
   }

   bson_iter_bson (&iter, &opts_from_test);
   BSON_ASSERT (bson_iter_init (&test_opts_iter, &opts_from_test));

   opts_from_uri = mongoc_uri_get_options (uri);
   creds_from_uri = mongoc_uri_get_credentials (uri);

   while (bson_iter_next (&test_opts_iter)) {
      opt_name = bson_iter_key (&test_opts_iter);
      opt_name_canon = mongoc_uri_canonicalize_option (opt_name);
      opts_or_creds = !bson_strcasecmp (opt_name, "authSource") ? creds_from_uri
                                                                : opts_from_uri;
      if (!bson_iter_init_find_case (
             &uri_opts_iter, opts_or_creds, opt_name_canon)) {
         fprintf (stderr,
                  "URI options incorrectly set from TXT record: "
                  "no option named \"%s\"\n"
                  "expected: %s\n"
                  "actual: %s\n",
                  opt_name,
                  bson_as_json (&opts_from_test, NULL),
                  bson_as_json (opts_or_creds, NULL));
         abort ();
      }

      test_value = bson_iter_value (&test_opts_iter);
      uri_value = bson_iter_value (&uri_opts_iter);
      if (!match_bson_value (uri_value, test_value, &ctx)) {
         fprintf (stderr,
                  "URI option \"%s\" incorrectly set from TXT record: %s\n"
                  "expected: %s\n"
                  "actual: %s\n",
                  opt_name,
                  ctx.errmsg,
                  bson_as_json (&opts_from_test, NULL),
                  bson_as_json (opts_from_uri, NULL));
         abort ();
      }
   }
}


typedef struct {
   bson_mutex_t mutex;
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

   bson_mutex_lock (&ctx->mutex);
   _mongoc_host_list_destroy_all (ctx->hosts);
   ctx->hosts = NULL;
   for (i = 0; i < n; i++) {
      ctx->hosts = _mongoc_host_list_push (
         sds[i]->host.host, sds[i]->host.port, AF_UNSPEC, ctx->hosts);
   }
   bson_mutex_unlock (&ctx->mutex);

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
_host_list_matches (const bson_t *test, context_t *ctx)
{
   bson_iter_t iter;
   bson_iter_t hosts;
   const char *host_and_port;
   bool ret = true;

   BSON_ASSERT (bson_iter_init_find (&iter, test, "hosts"));
   BSON_ASSERT (bson_iter_recurse (&iter, &hosts));

   bson_mutex_lock (&ctx->mutex);
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
   bson_mutex_unlock (&ctx->mutex);

   return ret;
}

typedef struct {
   const char *uri_str;
   const char *reason;
} skipped_dns_test_t;

skipped_dns_test_t SKIPPED_DNS_TESTS[] = {
   {"mongodb+srv://test5.test.build.10gen.cc/?authSource=otherDB",
    "C driver requires username present if any auth fields are present"},
   {0}};

static bool
is_test_skipped (const char *uri_str)
{
   skipped_dns_test_t *skip;

   for (skip = SKIPPED_DNS_TESTS; skip->uri_str != NULL; skip++) {
      if (!strcmp (skip->uri_str, uri_str)) {
         MONGOC_DEBUG (
            "Skipping test of URI: %s Reason: %s", skip->uri_str, skip->reason);
         return true;
      }
   }

   return false;
}

static void
_test_dns_maybe_pooled (bson_t *test, bool pooled)
{
   context_t ctx;
   bool expect_ssl;
   bool expect_error;
   mongoc_uri_t *uri;
   mongoc_apm_callbacks_t *callbacks;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t ssl_opts;
#endif
   int n_hosts;
   bson_error_t error;
   bool r;
   const char *uri_str;

   if (!test_framework_get_ssl ()) {
      fprintf (stderr,
               "Must configure an SSL replica set and set MONGOC_TEST_SSL=on "
               "and other ssl options to test DNS\n");
      abort ();
   }

   uri_str = bson_lookup_utf8 (test, "uri");
   if (is_test_skipped (uri_str)) {
      return;
   }

   bson_mutex_init (&ctx.mutex);
   ctx.hosts = NULL;
   expect_ssl = strstr (uri_str, "ssl=false") == NULL;
   expect_error = _mongoc_lookup_bool (test, "error", false /* default */);

   uri = mongoc_uri_new_with_error (uri_str, &error);
   if (!expect_error) {
      ASSERT_OR_PRINT (uri, error);
   }

   if (!uri) {
      /* expected failure, e.g. we're testing an invalid URI */
      return;
   }

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_topology_changed_cb (callbacks, topology_changed);

   /* suppress "cannot override URI option" messages */
   capture_logs (true);

#ifdef MONGOC_ENABLE_SSL
   ssl_opts = *test_framework_get_ssl_opts ();
   ssl_opts.allow_invalid_hostname = true;
#endif

   if (pooled) {
      pool = test_framework_client_pool_new_from_uri (uri, NULL);

      /* before we set SSL on so that we can connect to the test replica set,
       * assert that the URI has SSL on by default, and SSL off if "ssl=false"
       * is in the URI string */
      BSON_ASSERT (
         mongoc_uri_get_tls (_mongoc_client_pool_get_topology (pool)->uri) ==
         expect_ssl);
#ifdef MONGOC_ENABLE_SSL
      mongoc_client_pool_set_ssl_opts (pool, &ssl_opts);
#else
      test_framework_set_pool_ssl_opts (pool);
#endif
      mongoc_client_pool_set_apm_callbacks (pool, callbacks, &ctx);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = test_framework_client_new_from_uri (uri, NULL);
      BSON_ASSERT (mongoc_uri_get_tls (client->uri) == expect_ssl);
#ifdef MONGOC_ENABLE_SSL
      mongoc_client_set_ssl_opts (client, &ssl_opts);
#else
      test_framework_set_ssl_opts (client);
#endif
      mongoc_client_set_apm_callbacks (client, callbacks, &ctx);
   }

#ifdef MONGOC_ENABLE_SSL
   BSON_ASSERT (client->ssl_opts.allow_invalid_hostname);
#endif

   n_hosts = hosts_count (test);

   if (pooled) {
      if (n_hosts && !expect_error) {
         WAIT_UNTIL (_host_list_matches (test, &ctx));
      } else {
         r = mongoc_client_command_simple (
            client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
         BSON_ASSERT (!r);
         ASSERT_ERROR_CONTAINS (error,
                                MONGOC_ERROR_SERVER_SELECTION,
                                MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                                "");
      }
   } else if (NULL == mongoc_uri_get_username (uri)) {
      /* Skip single-threaded tests containing auth credentials. Monitoring
       * connections need to authenticate, and the credentials in the tests do
       * not correspond to the test users. TODO (CDRIVER-4046): unskip these
       * tests. */
      if (n_hosts && !expect_error) {
         r = mongoc_client_command_simple (
            client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
         ASSERT_OR_PRINT (r, error);
         WAIT_UNTIL (_host_list_matches (test, &ctx));
      } else {
         r = mongoc_client_command_simple (
            client, "admin", tmp_bson ("{'ping': 1}"), NULL, NULL, &error);
         BSON_ASSERT (!r);
         ASSERT_ERROR_CONTAINS (error,
                                MONGOC_ERROR_SERVER_SELECTION,
                                MONGOC_ERROR_SERVER_SELECTION_FAILURE,
                                "");
      }
   }

   /* the client's URI is updated after initial seedlist discovery (though for
    * background SRV polling, only the topology's URI is updated). Check that
    * both the topology and client URI have the expected options. */
   _assert_options_match (test, client->uri);
   _assert_options_match (test, client->topology->uri);

   /* the client has a copy of the topology's URI, assert they're the same */
   ASSERT (bson_equal (mongoc_uri_get_options (client->uri),
                       mongoc_uri_get_options (client->topology->uri)));
   ASSERT (bson_equal (mongoc_uri_get_credentials (client->uri),
                       mongoc_uri_get_credentials (client->topology->uri)));
   if (!mongoc_uri_get_hosts (client->uri)) {
      ASSERT (!mongoc_uri_get_hosts (client->topology->uri));
   } else {
      _mongoc_host_list_compare_one (
         mongoc_uri_get_hosts (client->uri),
         mongoc_uri_get_hosts (client->topology->uri));
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
test_dns (bson_t *test)
{
   _test_dns_maybe_pooled (test, false);
   _test_dns_maybe_pooled (test, true);
}


static int
test_dns_check_replset (void)
{
   return test_framework_getenv_bool ("MONGOC_TEST_DNS") ? 1 : 0;
}

static int
test_dns_check_loadbalanced (void)
{
   return test_framework_getenv_bool ("MONGOC_TEST_DNS_LOADBALANCED") ? 1 : 0;
}


/* ensure mongoc_topology_select_server_id handles a NULL error pointer in the
 * code path it follows when the topology scanner is invalid */
static void
test_null_error_pointer (void *ctx)
{
   mongoc_client_t *client;

   client =
      test_framework_client_new ("mongodb+srv://doesntexist.example.com", NULL);
   ASSERT (!mongoc_topology_select_server_id (client->topology,
                                              MONGOC_SS_READ,
                                              NULL /* read prefs */,
                                              NULL /* error */));

   mongoc_client_destroy (client);
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

   test_framework_resolve_path (
      JSON_DIR "/initial_dns_seedlist_discovery/replica-set", resolved);
   install_json_test_suite_with_check (suite,
                                       resolved,
                                       test_dns,
                                       test_dns_check_replset,
                                       test_framework_skip_if_no_crypto);

   test_framework_resolve_path (
      JSON_DIR "/initial_dns_seedlist_discovery/load-balanced", resolved);
   install_json_test_suite_with_check (suite,
                                       resolved,
                                       test_dns,
                                       test_dns_check_loadbalanced,
                                       test_framework_skip_if_no_crypto);
}

extern bool
mongoc_topology_apply_scanned_srv_hosts (mongoc_uri_t *uri,
                                         mongoc_topology_description_t *td,
                                         mongoc_host_list_t *hosts,
                                         bson_error_t *error);

static mongoc_host_list_t *
make_hosts (char *first_host, ...)
{
   va_list va;
   mongoc_host_list_t *hosts = NULL;
   mongoc_host_list_t host;
   char *host_str;

   _mongoc_host_list_from_string (&host, first_host);
   _mongoc_host_list_upsert (&hosts, &host);

   va_start (va, first_host);
   while ((host_str = va_arg (va, char *))) {
      _mongoc_host_list_from_string (&host, host_str);
      _mongoc_host_list_upsert (&hosts, &host);
   }
   va_end (va);
   return hosts;
}

#define MAKE_HOSTS(...) make_hosts (__VA_ARGS__, NULL)

static void
dump_hosts (mongoc_host_list_t *hosts)
{
   mongoc_host_list_t *host;

   MONGOC_DEBUG ("hosts:");
   for (host = hosts; host; host = hosts->next) {
      MONGOC_DEBUG ("- %s", host->host_and_port);
   }
}

static void
dump_topology_description (mongoc_topology_description_t *td)
{
   size_t i;
   mongoc_server_description_t *sd;

   MONGOC_DEBUG ("topology hosts:");
   for (i = 0; i < td->servers->items_len; ++i) {
      sd = (mongoc_server_description_t *) mongoc_set_get_item (td->servers,
                                                                (int) i);
      MONGOC_DEBUG ("- %s", sd->host.host_and_port);
   }
}

static void
check_topology_description (mongoc_topology_description_t *td,
                            mongoc_host_list_t *hosts)
{
   int nhosts = 0;
   mongoc_host_list_t *host;

   for (host = hosts; host; host = host->next) {
      uint32_t server_count;

      nhosts++;
      /* Check that "host" is already in the topology description by upserting
       * it, and ensuring that the number of servers remains constant. */
      server_count = td->servers->items_len;
      BSON_ASSERT (mongoc_topology_description_add_server (
         td, host->host_and_port, NULL));
      if (server_count != td->servers->items_len) {
         dump_topology_description (td);
         dump_hosts (hosts);
         test_error ("topology description did not have host: %s",
                     host->host_and_port);
      }
   }

   if (nhosts != td->servers->items_len) {
      dump_topology_description (td);
      dump_hosts (hosts);
      test_error ("topology description had extra hosts");
   }
}

static void
test_srv_polling_mocked (void *unused)
{
   mongoc_uri_t *uri;
   mongoc_topology_description_t td;
   bson_error_t error;
   mongoc_host_list_t *hosts;
   mongoc_host_list_t *expected;
   bool ret;

   mongoc_topology_description_init (&td, 0);
   uri = mongoc_uri_new ("mongodb+srv://server.test.com/?tls=true");
   capture_logs (true);

   hosts = MAKE_HOSTS ("a.test.com", "b.test.com");
   expected = MAKE_HOSTS ("a.test.com", "b.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, hosts, &error);
   ASSERT_OR_PRINT (ret, error);
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   _mongoc_host_list_destroy_all (hosts);
   ASSERT_NO_CAPTURED_LOGS ("topology");

   /* Add an extra host. */
   hosts = MAKE_HOSTS ("x.test.com", "a.test.com", "y.test.com", "b.test.com");
   expected =
      MAKE_HOSTS ("x.test.com", "a.test.com", "y.test.com", "b.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, hosts, &error);
   ASSERT_OR_PRINT (ret, error);
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   _mongoc_host_list_destroy_all (hosts);
   ASSERT_NO_CAPTURED_LOGS ("topology");

   /* Remove all but one host. */
   hosts = MAKE_HOSTS ("x.test.com");
   expected = MAKE_HOSTS ("x.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, hosts, &error);
   ASSERT_OR_PRINT (ret, error);
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   _mongoc_host_list_destroy_all (hosts);
   ASSERT_NO_CAPTURED_LOGS ("topology");

   /* Add one valid and one invalid. Invalid should skip, warning should be
    * logged. */
   hosts = MAKE_HOSTS ("x.test.com", "y.test.com", "bad.wrongdomain.com");
   expected = MAKE_HOSTS ("x.test.com", "y.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, hosts, &error);
   ASSERT_OR_PRINT (ret, error);
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   _mongoc_host_list_destroy_all (hosts);
   ASSERT_CAPTURED_LOG ("topology", MONGOC_LOG_LEVEL_ERROR, "Invalid host");

   /* An empty host list returns false but does NOT change topology description
    */
   expected = MAKE_HOSTS ("x.test.com", "y.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, NULL, &error);
   BSON_ASSERT (!ret);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_STREAM,
                          MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                          "SRV response did not contain any valid hosts");
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   ASSERT_CAPTURED_LOG ("topology", MONGOC_LOG_LEVEL_ERROR, "Invalid host");

   /* All invalid hosts returns false but does NOT change topology description
    */
   hosts = MAKE_HOSTS ("bad1.wrongdomain.com", "bad2.wrongdomain.com");
   expected = MAKE_HOSTS ("x.test.com", "y.test.com");
   ret = mongoc_topology_apply_scanned_srv_hosts (uri, &td, NULL, &error);
   BSON_ASSERT (!ret);
   ASSERT_ERROR_CONTAINS (error,
                          MONGOC_ERROR_STREAM,
                          MONGOC_ERROR_STREAM_NAME_RESOLUTION,
                          "SRV response did not contain any valid hosts");
   check_topology_description (&td, expected);
   _mongoc_host_list_destroy_all (expected);
   _mongoc_host_list_destroy_all (hosts);
   ASSERT_CAPTURED_LOG ("topology", MONGOC_LOG_LEVEL_ERROR, "Invalid host");

   mongoc_topology_description_destroy (&td);
   mongoc_uri_destroy (uri);
}

static void
test_small_initial_buffer (void *unused)
{
   mongoc_rr_type_t rr_type = MONGOC_RR_SRV;
   mongoc_rr_data_t rr_data;
   bson_error_t error;
   /* Size needs to be large enough to fit DNS answer header to not error, but
    * smaller than SRV response to test. The SRV response is 155 bytes. This can
    * be determined with: dig -t SRV _mongodb._tcp.test1.test.build.10gen.cc */
   size_t small_buffer_size = 30;

   memset (&rr_data, 0, sizeof (rr_data));
   ASSERT_OR_PRINT (
      _mongoc_client_get_rr ("_mongodb._tcp.test1.test.build.10gen.cc",
                             rr_type,
                             &rr_data,
                             small_buffer_size,
                             &error),
      error);
   ASSERT_CMPINT (rr_data.count, ==, 2);
   bson_free (rr_data.txt_record_opts);
   _mongoc_host_list_destroy_all (rr_data.hosts);
}

bool
_mock_resolver (const char *service,
                mongoc_rr_type_t rr_type,
                mongoc_rr_data_t *rr_data,
                size_t initial_buffer_size,
                bson_error_t *error)
{
   test_error ("Expected mock resolver to not be called");
   return true;
}

static void
_prose_loadbalanced_ping (mongoc_client_t *client)
{
   bson_error_t error;
   bson_t *cmd = BCON_NEW ("ping", BCON_INT32 (1));

   if (!mongoc_client_command_simple (
          client, "admin", cmd, NULL, NULL, &error)) {
      test_error ("ping failed: %s", error.message);
   }

   bson_destroy (cmd);
}

/*
   Implements prose test 9 as described in the SRV polling test README:
   Test that SRV polling is not done for load balanced clusters. Connect to
   mongodb+srv://test3.test.build.10gen.cc/?loadBalanced=true, mock the addition
   of the following DNS record, wait until 2*rescanSRVIntervalMS, and assert
   that the final topology description only contains one server
   (localhost.test.build.10gen.cc. at port 27017).
   _mongodb._tcp.test3.test.build.10gen.cc. 86400 IN SRV 27018
   localhost.test.build.10gen.cc.
*/
static void
_prose_test_9 (bool pooled)
{
   mongoc_client_pool_t *pool;
   mongoc_uri_t *uri;
   mongoc_client_t *client;
   mongoc_host_list_t *expected_hosts;
   mongoc_topology_t *topology;
#define RESCAN_INTERVAL_MS 500
#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t ssl_opts = *test_framework_get_ssl_opts ();

   ssl_opts.allow_invalid_hostname = true;
#endif


   uri = mongoc_uri_new ("mongodb+srv://test3.test.build.10gen.cc");
   mongoc_uri_set_option_as_bool (uri, MONGOC_URI_LOADBALANCED, true);
   /* Single-threaded clients will only enter SRV polling during monitoring in
    * mongoc_topology_scan_once. Reducing the heartbeatFrequencyMS will exercise
    * the code path that would poll for SRV records. That should be bypassed
    * because of the load balanced topology type. */
   mongoc_uri_set_option_as_int32 (
      uri, MONGOC_URI_HEARTBEATFREQUENCYMS, RESCAN_INTERVAL_MS);

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      topology = _mongoc_client_pool_get_topology (pool);

#ifdef MONGOC_ENABLE_SSL
      mongoc_client_pool_set_ssl_opts (pool, &ssl_opts);
#endif

   } else {
      client = mongoc_client_new_from_uri (uri);
#ifdef MONGOC_ENABLE_SSL
      mongoc_client_set_ssl_opts (client, &ssl_opts);
#endif
      topology = client->topology;
   }

   bson_mutex_lock (&topology->mutex);
   _mongoc_topology_set_rr_resolver (topology, _mock_resolver);
   _mongoc_topology_set_srv_polling_rescan_interval_ms (topology,
                                                        RESCAN_INTERVAL_MS);
   bson_mutex_unlock (&topology->mutex);

   if (pooled) {
      client = mongoc_client_pool_pop (pool);
   }

   _mongoc_usleep (2 * RESCAN_INTERVAL_MS * 1000);
   /* For single-threaded, perform an operation since SRV polling occurs as a
    * part of topology scanning. */
   if (!pooled) {
      _prose_loadbalanced_ping (client);
   }

   bson_mutex_lock (&topology->mutex);
   expected_hosts = MAKE_HOSTS ("localhost.test.build.10gen.cc:27017");
   check_topology_description (&client->topology->description, expected_hosts);
   bson_mutex_unlock (&topology->mutex);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   mongoc_uri_destroy (uri);
   _mongoc_host_list_destroy_all (expected_hosts);
}

static void
prose_test_9_single (void *unused)
{
   _prose_test_9 (false);
}

static void
prose_test_9_pooled (void *unused)
{
   _prose_test_9 (true);
}

void
test_dns_install (TestSuite *suite)
{
   test_all_spec_tests (suite);
   TestSuite_AddFull (suite,
                      "/initial_dns_seedlist_discovery/null_error_pointer",
                      test_null_error_pointer,
                      NULL,
                      NULL,
                      test_framework_skip_if_no_crypto);
   TestSuite_AddFull (suite,
                      "/initial_dns_seedlist_discovery/srv_polling/mocked",
                      test_srv_polling_mocked,
                      NULL,
                      NULL,
                      NULL);
   TestSuite_AddFull (suite,
                      "/initial_dns_seedlist_discovery/small_initial_buffer",
                      test_small_initial_buffer,
                      NULL,
                      NULL,
                      test_dns_check_replset);

   /* TODO (CDRIVER-4045): remove /initial_dns_seedlist_discovery from the path
    * of the SRV polling tests, since they are defined in the "Polling SRV
    * Records for mongos Discovery" spec, not the "Initial DNS Seedlist
    * Discovery" spec. */
   TestSuite_AddFull (
      suite,
      "/initial_dns_seedlist_discovery/srv_polling/prose_test_9/single",
      prose_test_9_single,
      NULL,
      NULL,
      test_dns_check_loadbalanced);

   TestSuite_AddFull (
      suite,
      "/initial_dns_seedlist_discovery/srv_polling/prose_test_9/pooled",
      prose_test_9_pooled,
      NULL,
      NULL,
      test_dns_check_loadbalanced);
}
