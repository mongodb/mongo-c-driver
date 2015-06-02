#include <mongoc.h>

#include "mongoc-topology-scanner-private.h"
#include "mock_server2/mock-server2.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "topology-scanner-test"

#define TIMEOUT 20000
#define NSERVERS 10

#define TRUST_DIR "tests/trust_dir"
#define CAFILE TRUST_DIR "/verify/mongo_root.pem"
#define PEMFILE_NOPASS TRUST_DIR "/keys/mongodb.com.pem"

static bool
test_topology_scanner_helper (uint32_t      id,
                              const bson_t *bson,
                              int64_t       rtt_msec,
                              void         *data,
                              bson_error_t *error)
{
   bson_iter_t iter;
   int *finished = (int*)data;

   if (error->code) {
      fprintf (stderr, "scanner error: %s\n", error->message);
      abort ();
   }

   /* mock servers are configured to return their ids as max wire version */
   assert (bson);
   assert (bson_iter_init_find (&iter, bson, "maxWireVersion"));
   assert (BSON_ITER_HOLDS_INT32 (&iter));
   uint32_t max_wire_version = (uint32_t) bson_iter_int32 (&iter);
   ASSERT_CMPINT (max_wire_version, ==, id);

   (*finished)--;

   return *finished >= NSERVERS ? true : false;
}

static void
_test_topology_scanner(bool with_ssl)
{
   mock_server2_t *servers[NSERVERS];
   mongoc_topology_scanner_t *topology_scanner;
   int i;
   bson_t q = BSON_INITIALIZER;
   int finished = NSERVERS * 3;
   bool more_to_do;

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t sopt = { 0 };
   mongoc_ssl_opt_t copt = { 0 };
#endif

   topology_scanner = mongoc_topology_scanner_new (
         NULL, &test_topology_scanner_helper, &finished);

#ifdef MONGOC_ENABLE_SSL
   if (with_ssl) {
      copt.ca_file = CAFILE;
      copt.weak_cert_validation = 1;

      mongoc_topology_scanner_set_ssl_opts (topology_scanner, &copt);
   }
#endif

   for (i = 0; i < NSERVERS; i++) {
      /* use max wire versions just to distinguish among responses */
      servers[i] = mock_server2_with_autoismaster (i);
      mock_server2_set_rand_delay (servers[i], true);

#ifdef MONGOC_ENABLE_SSL
      if (with_ssl) {
         sopt.pem_file = PEMFILE_NOPASS;
         sopt.ca_file = CAFILE;

         mock_server2_set_ssl_opts (servers[i], &sopt);
      }
#endif

      mock_server2_run (servers[i]);

      mongoc_topology_scanner_add(
            topology_scanner,
            mongoc_uri_get_hosts (mock_server2_get_uri (servers[i])),
            (uint32_t) i);
   }

   for (i = 0; i < 3; i++) {
      mongoc_topology_scanner_start (topology_scanner, TIMEOUT);

      more_to_do = mongoc_topology_scanner_work (topology_scanner, TIMEOUT);

      assert(! more_to_do);
   }

   assert(finished == 0);

   mongoc_topology_scanner_destroy (topology_scanner);

   bson_destroy (&q);

   for (i = 0; i < NSERVERS; i++) {
      mock_server2_destroy (servers[i]);
   }
}


void
test_topology_scanner ()
{
   _test_topology_scanner (false);
}


#ifdef MONGOC_ENABLE_SSL
void
test_topology_scanner_ssl ()
{
   _test_topology_scanner (true);
}
#endif


void
test_topology_scanner_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/TOPOLOGY/scanner", test_topology_scanner);
#ifdef MONGOC_ENABLE_SSL
   TestSuite_Add (suite, "/TOPOLOGY/scanner_ssl", test_topology_scanner_ssl);
#endif
}
