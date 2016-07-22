#include <mongoc.h>

#include "mock_server/mock-server.h"
#include "mock_server/future-functions.h"
#include "test-conveniences.h"
#include "TestSuite.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "async-test"

#define NSERVERS 10


static mongoc_uri_t *
uri_for_ports (uint16_t ports[NSERVERS])
{
   bson_string_t *str = bson_string_new ("mongodb://");
   int i;
   mongoc_uri_t *uri;

   bson_string_append_printf (str, "localhost:%hu", ports[0]);

   for (i = 1; i < NSERVERS; i++) {
      bson_string_append_printf (str, ",localhost:%hu", ports[i]);
   }

   bson_string_append_printf (str, "/?replicaSet=rs");
   uri = mongoc_uri_new (str->str);
   assert (uri);

   bson_string_free (str, true);

   return uri;
}


static void
shuffle (mock_server_t *servers[NSERVERS])
{
   size_t i;
   mock_server_t *s;

   for (i = 0; i < NSERVERS - 1; i++) {
      size_t j = i + rand () / (RAND_MAX / (NSERVERS - i) + 1);
      s = servers[j];
      servers[j] = servers[i];
      servers[i] = s;
   }
}


static void
_test_ismaster (bool with_ssl,
                bool pooled)
{
   const char *secondary_reply = "{'ok': 1, 'ismaster': false,"
                                 " 'secondary': true, 'setName': 'rs'}";

   mock_server_t *servers[NSERVERS];
   mongoc_uri_t *uri;
   mongoc_client_pool_t *pool = NULL;
   mongoc_client_t *client;
   mongoc_read_prefs_t *read_prefs;
   bson_error_t error;
   future_t *future;
   request_t *request;
   uint16_t ports[NSERVERS];
   int i;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t sopt = { 0 };
   mongoc_ssl_opt_t copt = { 0 };
#endif

   for (i = 0; i < NSERVERS; i++) {
      servers[i] = mock_server_new ();

#ifdef MONGOC_ENABLE_SSL
      if (with_ssl) {
         sopt.weak_cert_validation = true;
         sopt.pem_file = CERT_SERVER;
         sopt.ca_file = CERT_CA;

         mock_server_set_ssl_opts (servers[i], &sopt);
      }
#endif

      ports[i] = mock_server_run (servers[i]);
      assert (ports[i]);
   }

   uri = uri_for_ports (ports);

   if (with_ssl) {
#ifdef MONGOC_ENABLE_SSL
      copt.ca_file = CERT_CA;
      copt.weak_cert_validation = 1;
      if (pooled) {
         pool = mongoc_client_pool_new (uri);
         mongoc_client_pool_set_ssl_opts (pool, &copt);
         client = mongoc_client_pool_pop (pool);
      } else {
         client = mongoc_client_new_from_uri (uri);
         mongoc_client_set_ssl_opts (client, &copt);
      }
#endif
   } else {
      if (pooled) {
         pool = mongoc_client_pool_new (uri);
         client = mongoc_client_pool_pop (pool);
      } else {
         client = mongoc_client_new_from_uri (uri);
      }
   }

   read_prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY);

   future = future_client_command_simple (client, "test",
                                          tmp_bson ("{'ping': 1}"),
                                          read_prefs, NULL, &error);

   /* prove scanner is async: we can respond to ismaster in any order */
   shuffle (servers);

   for (i = 0; i < NSERVERS; i++) {
      request = mock_server_receives_ismaster (servers[i]);
      assert (request);

      /* only 5th server in list is a suitable secondary */
      if (i == 5) {
         mock_server_replies_simple (request, secondary_reply);
         request_destroy (request);
      } else {
         /* replies "ok", is marked as type "standalone" and removed */
         mock_server_replies_ok_and_destroys (request);
      }
   }

   if (pooled) {
      /* client opens new connection and calls isMaster on it */
      request = mock_server_receives_ismaster (servers[5]);
      mock_server_replies_simple (request, secondary_reply);
      request_destroy (request);
   }

   request = mock_server_receives_command (
      servers[5], "test", MONGOC_QUERY_SLAVE_OK, "{'ping': 1}");

   mock_server_replies_ok_and_destroys (request);
   ASSERT_OR_PRINT (future_get_bool (future), error);
   future_destroy (future);

   mongoc_read_prefs_destroy (read_prefs);
   mongoc_uri_destroy (uri);

   if (pooled) {
      mongoc_client_pool_push (pool, client);
      mongoc_client_pool_destroy (pool);
   } else {
      mongoc_client_destroy (client);
   }

   for (i = 0; i < NSERVERS; i++) {
      mock_server_destroy (servers[i]);
   }
}


static void
test_ismaster (void)
{
   _test_ismaster (false, false);
}


static void
test_ismaster_pooled (void)
{
   _test_ismaster (false, true);
}


#ifdef MONGOC_ENABLE_SSL_OPENSSL
static void
test_ismaster_ssl (void)
{
   _test_ismaster (true, false);
}


static void
test_ismaster_ssl_pooled (void)
{
   _test_ismaster (true, true);
}
#endif


void
test_async_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/Async/ismaster", test_ismaster);
   TestSuite_Add (suite, "/Async/ismaster/pooled",
                  test_ismaster_pooled);

#ifdef MONGOC_ENABLE_SSL_OPENSSL
   TestSuite_Add (suite, "/Async/ismaster_ssl", test_ismaster_ssl);
   TestSuite_Add (suite, "/Async/ismaster_ssl/pooled",
                  test_ismaster_ssl_pooled);
#endif
}
