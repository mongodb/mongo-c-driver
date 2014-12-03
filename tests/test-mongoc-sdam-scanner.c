#include <mongoc.h>

#include "mongoc-sdam-scanner-private.h"
#include "mock-server.h"
#include "mongoc-tests.h"
#include "TestSuite.h"

#include "test-libmongoc.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "sdam-scanner-test"

#define TIMEOUT 1000
#define NSERVERS 100

#define TRUST_DIR "tests/trust_dir"
#define CAFILE TRUST_DIR "/verify/mongo_root.pem"
#define PEMFILE_NOPASS TRUST_DIR "/keys/mongodb.com.pem"

#ifdef _WIN32
static void
usleep (int64_t usec)
{
   HANDLE timer;
   LARGE_INTEGER ft;

   ft.QuadPart = -(10 * usec);

   timer = CreateWaitableTimer (NULL, true, NULL);
   SetWaitableTimer (timer, &ft, 0, NULL, NULL, 0);
   WaitForSingleObject (timer, INFINITE);
   CloseHandle (timer);
}
#endif

static bool
test_sdam_scanner_helper (uint32_t      id,
                          const bson_t *bson,
                          void         *data,
                          bson_error_t *error)
{
   int *finished = (int*)data;

   assert(bson);

   (*finished)--;

   return *finished >= 100 ? true : false;
}

static void
test_sdam_scanner(void)
{
   mock_server_t *servers[NSERVERS];
   mongoc_sdam_scanner_t *sdam_scanner;
   uint16_t port;
   int i;
   bson_t q = BSON_INITIALIZER;
   int finished = NSERVERS * 3;
   mongoc_host_list_t host = { 0 };

#ifdef MONGOC_ENABLE_SSL
   mongoc_ssl_opt_t sopt = { 0 };
   mongoc_ssl_opt_t copt = { 0 };
#endif

   port = 20000 + (rand () % 1000);

   sdam_scanner = mongoc_sdam_scanner_new (&test_sdam_scanner_helper, &finished);

#ifdef MONGOC_ENABLE_SSL
   copt.ca_file = CAFILE;
   copt.weak_cert_validation = 1;
   sdam_scanner->ssl_opts = &copt;
#endif

   for (i = 0; i < NSERVERS; i++) {
      servers[i] = mock_server_new ("127.0.0.1", port + i, NULL, NULL);
      mock_server_set_wire_version (servers[i], 0, i);

#ifdef MONGOC_ENABLE_SSL
      sopt.pem_file = PEMFILE_NOPASS;
      sopt.ca_file = CAFILE;

      mock_server_set_ssl_opts (servers[i], &sopt);
#endif

      mock_server_run_in_thread (servers[i]);

      snprintf(host.host, sizeof(host.host), "127.0.0.1");
      snprintf(host.host_and_port, sizeof(host.host_and_port), "127.0.0.1:%d", port + i);
      host.port = port + i;
      host.family = AF_INET;

      mongoc_sdam_scanner_add(sdam_scanner, &host);
   }

   usleep (5000);

   for (i = 0; i < 3; i++) {
      mongoc_sdam_scanner_scan (sdam_scanner, TIMEOUT);
   }

   assert(finished == 0);

   mongoc_sdam_scanner_destroy (sdam_scanner);

   bson_destroy (&q);

   for (i = 0; i < NSERVERS; i++) {
      mock_server_quit (servers[i], 0);
   }
}

void
test_sdam_scanner_install (TestSuite *suite)
{
   bool local;

   local = !getenv ("MONGOC_DISABLE_MOCK_SERVER");

   if (local) {
      TestSuite_Add (suite, "/SDAM/scanner", test_sdam_scanner);
   }
}
